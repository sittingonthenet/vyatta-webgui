#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <syslog.h>
#include <iostream>
#include "common.hh"
#include "multirespcmd.hh"

using namespace std;

/**
 *
 **/
MultiResponseCommand::MultiResponseCommand(string session_id, string &orig_cmd, string &cmd) : 
  _session_id(session_id), 
  _orig_cmd(orig_cmd), 
  _cmd(cmd)
{
  //read in valid cmd list
  load_valid_multi_cmds();
}

/**
 *
 **/
MultiResponseCommand::~MultiResponseCommand()
{

}

/**
 *
 **/
void
MultiResponseCommand::init()
{
  int servlen;
  struct sockaddr_un  serv_addr;

  bzero((char *)&serv_addr,sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  strcpy(serv_addr.sun_path, WebGUI::CHUNKER_SOCKET.c_str());
  servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
  if ((_sock = socket(AF_UNIX, SOCK_STREAM,0)) < 0) {
    cerr << "MultiResponseCommand::init(): error creating socket" << endl;
    return;
    //    error("Creating socket");
  }
  if (connect(_sock, (struct sockaddr *)&serv_addr, servlen) < 0) {
    cerr << "MultiResponseCommand::init(): error connecting to socket: " << errno << endl;
    return;
    //    error("Connecting");
  }
}

/**
 *
 **/
bool
MultiResponseCommand::process()
{
  //we'll take everything now...
  if (strncmp(WebGUI::CHUNKER_RESP_TOK_BASE.c_str(),_orig_cmd.c_str(),WebGUI::CHUNKER_RESP_TOK_BASE.length()) == 0) {
    _next_token = _orig_cmd;
  }
 
 if (_next_token.empty() == true) { //want to start a new command
    string id = start_new_proc();
    _next_token = WebGUI::CHUNKER_RESP_TOK_BASE + id + "_0";
    return true;
  }
  
  //then check if this matches a mult-part cmd
  string file_chunk = WebGUI::CHUNKER_RESP_TOK_DIR + "/" + _next_token;
  struct stat s;
  if ((lstat(file_chunk.c_str(), &s) == 0) && S_ISREG(s.st_mode)) {
    //found chunk now read next
    FILE *fp = fopen(file_chunk.c_str(), "r");
    if (fp) {
      char buf[1025];
      while (fgets(buf,1024,fp) != 0) {
	_resp += string(buf);
      }
      _next_token = get_next_resp_file(_next_token);
      fclose(fp);
      return true;
    }
  }
  else { //need to determine if the chunker is done, or the request needs to be resent
    //will look for end file, otherwise return true
    string end_file = WebGUI::CHUNKER_RESP_TOK_DIR + "/" + WebGUI::CHUNKER_RESP_TOK_BASE + _session_id + "_end";
    if ((lstat(end_file.c_str(), &s) == 0)) {
      //found chunk now read next
      _next_token = WebGUI::CHUNKER_RESP_TOK_BASE + _session_id + "_end";
    }
    return true;
  }
  return true; //want to return true here
}

/**
 *
 **/
string
MultiResponseCommand::start_new_proc()
{
  string tok = _session_id;
  char buffer[1024];
  bzero(buffer,1024);
  sprintf(buffer,WebGUI::CHUNKER_MSG_FORMAT.c_str(),tok.c_str(),_cmd.c_str());

  write(_sock,buffer,sizeof(buffer));
  usleep(1000*1000); //give this a 1 second delay on start
  return tok;
}

/**
 *
 **/
void
MultiResponseCommand::get_resp(string &token, string &output)
{
  string tok = _session_id;
  char buffer[1024];
  bzero(buffer,1024);
  sprintf(buffer,WebGUI::CHUNKER_UPDATE_FORMAT.c_str(),tok.c_str());

  write(_sock,buffer,sizeof(buffer));

  token = _next_token;
  output = _resp;
}

/**
 *
 **/
void
MultiResponseCommand::load_valid_multi_cmds()
{
  //read in conf file and stuff values into set
  FILE *fp = fopen(WebGUI::CHUNKER_RESP_CMDS.c_str(),"r"); 
  if (fp) {
    char buf[1025];
    while (fgets(buf,1024,fp) != 0) {
      string tmp(buf);
      int pos = tmp.find('#');
      if (pos > 0) {
	tmp = tmp.substr(0,pos);
      }
      pos = tmp.find('\n');
      if (pos > 0) {
	tmp = tmp.substr(0,pos);
      }
      //now if empty skip and make sure to drop '/n'
      if (tmp.empty() == false) {
	_cmd_coll.insert(tmp);
      }
    }
    fclose(fp);
  }
}

/**
 *
 **/
string
MultiResponseCommand::get_next_resp_file(std::string &tok)
{
  //find last character and add 1
  int pos = tok.rfind("_");
  string tmp = tok.substr(pos+1,tok.length());
  int val = strtoul(tmp.c_str(),NULL,10);
  ++val;
  char buf[80];
  sprintf(buf,"%d",val);
  return tok.substr(0,pos) + "_" + string(buf);
}
