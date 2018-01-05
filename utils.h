#ifndef UTILS_H
#define UTILS_H
#include <string>
#include <cstdlib>
#include <vector>
#include <map>
#include <iostream>

//check if variable name is valid:
bool validateVarName(std::string varname){
    //  cout <<"validating:"<< varname << "\n";
    size_t pos = varname.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_");
    if(pos ==std::string::npos){
      return true;
    }
    return false;
}

 //get PATH environment filepaths:
  std::pair<std::vector<char *>,int> get_env_fp(){
    std::vector<char*> env;
    char * path = getenv("PATH");
    //std::cout << path << "\n";
    std::string curr_env = "";
    int argc = 0;
    for(int i = 0; path[i] !='\0'; i++){
      if(path[i] ==':'){
	if(curr_env.size()>0){
	  env.push_back(const_cast<char*>(strdup(curr_env.c_str())));
	  curr_env = "";
	  argc++;
	}
      }else{
	curr_env+=path[i];
      }
    }
    if(curr_env.size() > 0){
      env.push_back(const_cast<char*>(strdup(curr_env.c_str())));
      curr_env = "";
      argc++;
    }
    
    
    return std::make_pair(env,argc);
  }
//search for redirection
  std::vector<int> search_redir(std::vector<char *> & argv, int argc, int std){
    std::string std_sym = "";
    std::vector<int> indices; 
    switch(std){
    case 0: // stdin
      std_sym = "<";
      break;
    case 1: //stdout
       std_sym = ">";
      break;
    case 2: //stderr
      std_sym = "2>";
      break;
    default:
      std_sym = "";
      break;
    }
    if(std_sym != ""){
      for(int i=1; i<argc; i++){
	std::string curr = std::string(argv.at(i));
	if(curr == std_sym){
	  indices.push_back(i);
	}
      }
    }
    return indices;
  }

 //search for any pipes:
  std::vector<int> search_pipes(std::vector<char*> & argv, int argc){
    std::vector<int> pipe_ind;
    for(int i=0; i<argc; i++){
      std::string curr = argv.at(i);
      if(curr.find('|') != std::string::npos){
	pipe_ind.push_back(i);
      }
    }
    return pipe_ind;
  }

 //split arguments up
  std::vector< std::vector<char*> > split_pipe_args(std::vector<char*> & argv, int argc){
    std::vector<int> pipe_ind = search_pipes(argv, argc);
    std::vector< std::vector<char*> > arguments; 
    if(pipe_ind.size() !=0){
      for(size_t i =0; i<=pipe_ind.size(); i++){
	if(i!=pipe_ind.size() && (pipe_ind.at(i) == 0 || pipe_ind.at(i) == (argc-1))){
	  break;
	}else{
	  if(i==0){
	    std::vector<char *> val(argv.begin(), argv.begin()+pipe_ind.at(i));
	    arguments.push_back(val);
	  }else if(i==pipe_ind.size()){
	    std::vector<char *>  val(argv.begin()+pipe_ind.at(i-1)+1, argv.end());
	    arguments.push_back(val);
	  }else{
	    std::vector<char *>  val(argv.begin()+pipe_ind.at(i-1)+1, argv.begin()+pipe_ind.at(i));
	    arguments.push_back(val);
	  }
	}
      }
    }
    if(pipe_ind.size() == 0 || arguments.size() == 0){
      arguments.push_back(argv);
    }
    return arguments;
  }
//search path directory for command
  std::string find_cmd(std::vector<char *> & env, std::string & cmd, int argc){
  
    for(int i=0; i<argc; i++){
      std::string curr_fp = env.at(i);
      curr_fp+= "/"+ cmd;
      std::ifstream file(curr_fp.c_str());
      if(file.good()){
	//cout << "FOUND:" << curr_fp << "\n";
	return curr_fp;
       }
    }
    std::cerr << "Command " << cmd << " not found\n";
    exit(EXIT_FAILURE);
  }
#endif
