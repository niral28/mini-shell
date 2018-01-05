#ifndef MYSHELL_H
#define MYSHELL_H
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <map>
#include "utils.h"
extern char ** environ;

class MyShell{
 private:
  //map to track shell variables
  std::map<std::string, std::string> shellVars;

  /*------------Memory Management section -----------*/
  //memory management
  void destroy(std::vector<char *> & args){
     for(size_t i = 0; i<args.size(); i++){
      	free(args.at(i));
      }
   }
  //handles memory properly upon exit
  void exit_gracefully(std::vector<char*> & argv,std::string err_message){
    std::cerr<<err_message;
    destroy(argv);
    exit(EXIT_FAILURE);
  }

  /* ------------ Environmental Variables + Shell Variables section --------*/
  
  //set shell variables (only persist for existence of shell
  int setShellVar(std::string & cmd, std::vector<char*> & argv){
    if(argv.size() !=3){
      std::cerr << "incorrect arg size:" << argv.size() << "\n";
    }
    std::string varname = argv[1];
    //cout << "varname:"<<varname<<"\n";
    if(!validateVarName(varname)){
      std::cerr << "invalid variable name!\n";
      return -1;
    }else{
      shellVars[varname] = argv[2];
    }
    return 0;
      //cout << shellVars[argv[1]] << "\n";
  }

  //handles exporting variables to environmental variable list:
  void exportVar(std::string key){
    if(shellVars.find(key) != shellVars.end()){
       int i = setenv(key.c_str(), shellVars[key].c_str(),1);
       if(i!=0){
	 std::cerr<<"unable to export variable:" << key << "\n";
       }
    }else{
      std::cerr << "variable:" << key << " does not exist\n";
    }
  }
 //load environmental variables into the shell:
  void loadEnv2Shell(){
    int i =0;
    char * curr = environ[i];
    while(curr !=NULL){
      std::string var = std::string(curr);
      std::string key = "";
      std::string val = "";
      size_t pos = var.find("=");
      if(pos!= std::string::npos){
	key =  var.substr(0,pos);
	val = var.substr(pos+1);
	shellVars[key] = val;
      }
      i++;
      curr = environ[i];
    }
  }

  /*------------------------Redirection Section--------------------*/
  //1 method defined in utils.h
  //search_redir - searches arguments for redir symbol (whitespace required)
   void open_file_redirect(std::vector<char*> & argv,int argc, std::vector<int> & search_list, int mode, std::vector<int> open_fd){
    int index = search_list.at(0);
    int permissions = O_WRONLY |O_CREAT| O_TRUNC;
    if(mode == 0){
      permissions = O_RDONLY;
    }
      if(index!=0 && index!= (argc-1)){ // can't be first or last arg
	//4 = read, 2= write, 1= execute (RW permissions)
	int out_fd = open(argv.at(index+1),permissions, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR); 
	if(out_fd != -1){
	  int retval = dup2(out_fd,mode);
	  if(retval == -1){
	    std::string m = ""+mode;
	    std::string err_msg = "can't dup file descriptor:"+m+ ",redirect failed\n";
	    exit_gracefully(argv,err_msg);
	  }
	  open_fd.push_back(out_fd);
	  char * temp = argv.at(index);
	  argv.at(index) = NULL;
	  free(temp);
	  temp = argv.at(index+1);
	  argv.at(index+1) = NULL;
	  free(temp);
	  close(out_fd);
        }else{
	  std::string err_msg = "can't write to file:"+std::string(argv.at(index+1)) + "\n";
	  return;
	  //exit_gracefully(argv,err_msg);
	}
      }else{
	exit_gracefully(argv,"invalid redirect");
      }
  }

  //check all forms of redirection:
  std::vector<int> redirect(std::string & cmd, std::vector<char*> & argv, int argc){
    std::vector<int> open_fd;
    //currently only looking at first index of each
    std::vector<int> search_stdout = search_redir(argv,argc,1);
    std::vector<int> search_stdin = search_redir(argv,argc,0);
    std::vector<int> search_stderr = search_redir(argv,argc,2);

    if(search_stdout.size() > 0){ //fd: 1
      open_file_redirect(argv,argc,search_stdout, 1, open_fd);
    }
    if(search_stdin.size() > 0){ // fd: 0
       open_file_redirect(argv,argc,search_stdin,0,open_fd);
    }
    if(search_stderr.size() > 0){ // fd: 2
      open_file_redirect(argv,argc,search_stderr,2,open_fd);
    }
     return open_fd;
  }

  
  void closeFiles(std::vector< std::vector<int> > open_fd){
    for(size_t i=0; i<open_fd.size(); i++){
      for(size_t j=0; j<open_fd.at(i).size(); j++){
	close(open_fd.at(i).at(j));
      }
    }
  }
  /*----------------------------Pipes Section----------------------*/
  //Whitespace required to split pipes
  //2 pipe utility functions defined in utils.h 
  //search_pipes (find's indices of pipes) 
  //split_pipe_args (splits arguments divided by pipes)

  //allocate pipes for running command
  std::vector<int *> alloc_pipes(size_t args_v_size){
   //piped stuff
    std::vector<int *> pipes;
      for(size_t i =0; i<args_v_size-1; i++){ // number of pipes = (args-1)
	int * curr_pipe = new int[2];
	int retval = pipe(curr_pipe);
	if(retval == -1){
	  std::cerr << "failed to pipe\n";
	}else{
	  pipes.push_back(curr_pipe);
	}
      }
      return pipes;
   }

  //close pipes
  void close_pipes(std::vector<int *> pipes){
    for(size_t i=0; i<pipes.size(); i++){
      close(pipes.at(i)[0]);
      close(pipes.at(i)[1]);
    }
  }
  
  /*-------------------------Section: Running Commands ------------*/
  //1 utility function defined in util.h
  //find_cmd (find's command in PATH)


  /*------------------Sub Section: Built-In Commands--------------*/
  //commands built in:
  //eg. cd, set, export, (exit- though not handled in this section)

  //method implements cd command:
  int changeDir(std::string & path){
    if(path == "~"){
      path = getenv("HOME");
    }
    int i = chdir(path.c_str());
    if(i!=0){
      std::string err = "cd to  "+path+" failed ";
      perror(err.c_str());
    }
    return i;
  }

  //commands that are built into the shell (no need to fork)
  int builtInCommands(std::string & cmd, std::vector<char*> & argv, int argc){
    int retval = 0;
    if(cmd.compare("cd") == 0){
    //execute cd stuff
      if(argc < 2){
	std::cerr << "only 1 argument specified\n";
	retval = -1;
      }else{
	std::string path = argv[1];
	retval= changeDir(path);
      }
    }else if(cmd.compare("set") ==0){ //set variables	
      if(argc >= 3){
        retval=setShellVar(cmd, argv);
      }else{
	std::cerr << "not enough arguments,"<<argc<<" given,3 required\n";
	retval = -1;
      }
    }else if(cmd.compare("export") == 0){
      if(argc !=2){
	std::cerr << "not enough arguments, expect 2," << argc <<" given\n";
	retval = -1;
      }else{
	std::string key= std::string(argv[1]);
	exportVar(key);
      }
    }
    destroy(argv);
    return retval;
  }
  /*--------------------Run Regular-Non-Built-In Commands -----------------*/
  //run multiple proccesses:
  int runMulti(std::string cmd, std::vector<char*> & argv, int argc){ 
    if(cmd.compare("exit") ==0 || cmd.compare("cd") == 0 || cmd.compare("set") == 0 || cmd.compare("export") == 0){
        return builtInCommands(cmd, argv,argc);
    }
    pid_t  cpid,w;
    int wstatus;
    std::vector< std::vector<int> >  open_fd;
    std::vector< std::vector<char*> > args_v = split_pipe_args(argv,argc); 
    size_t sz = args_v.size();
    std::vector<int *> pipes = alloc_pipes(sz);
    size_t i=0;

    for(; i<sz; i++){
       cpid = fork();
       if(cpid == -1){
	 perror("fork");
	  exit(EXIT_FAILURE);
       }if(cpid == 0){
	 if(sz ==1){
	   run_command(args_v.at(i),open_fd,-1,-1,-1,-1, pipes);
	 }else{ // pipes exist 
	   if(i==0){ // if its the first one
	     run_command(args_v.at(i),open_fd,-1,-1,1,pipes.at(i)[1],pipes);
	   }else if(i==(sz-1)){//if its the last one
	     run_command(args_v.at(i),open_fd, 0,pipes.at(i-1)[0],-1,-1, pipes);
	   }else{
	     run_command(args_v.at(i),open_fd, 0,pipes.at(i-1)[0],1, pipes.at(i)[1], pipes);
	   }
	 }
       }
    }
    //only the parent reaches here:
    while((w=wait(&wstatus))>0){
      close_pipes(pipes);
    }
    //output status only of last command proccesed
    if(WIFEXITED(wstatus)){
      std::cerr << "Program exited with status " << WEXITSTATUS(wstatus)<<"\n" ;
    }else if(WIFSIGNALED(wstatus)){
      std::cerr<< "Program was killed by signal " <<WTERMSIG(wstatus)<<"\n" ; 
    }
    close_pipes(pipes);

    //delete pipes
    for(size_t i=0; i<pipes.size(); i++){
      int * x = pipes.at(i);
      delete[] x;
    }
    closeFiles(open_fd);
    destroy(argv);
    return wstatus;
  }

  //this is something that all child proccess run
  int run_command(std::vector< char *> & argv, std::vector< std::vector<int> > open_fd, int pipe_r,
		  int replace_pipe_r, int pipe_w, int replace_pipe_w, std::vector<int*> & pipes){
      std::string cmd  = argv.at(0);
      size_t  argc = argv.size();
      char ** data = NULL;
      
     if(cmd.compare("exit") ==0 || cmd.compare("cd") == 0 || cmd.compare("set") == 0 || cmd.compare("export") == 0){
        return builtInCommands(cmd, argv,argc);
     }
      else{
	if(cmd.size() <1){
	  std::cerr << "not enough arguments\n";
	  exit(EXIT_FAILURE);
	}else if(cmd == "exit"){
	  destroy(argv);
	  exit(EXIT_SUCCESS);
	}
        if(pipe_r != -1){
	  dup2(replace_pipe_r, pipe_r);
	}
	if(pipe_w != -1){
	  dup2(replace_pipe_w, pipe_w);
	}
	
	open_fd.push_back(redirect(cmd,argv,argc));
	data = new char *[argc+1];
	for(size_t i=0; i<argc; i++){
	  data[i] = argv.at(i);
	}
	data[argc] = NULL;
	//if path specified in command, let it run
	if(cmd.find("/") != std::string::npos){
	  close_pipes(pipes);
	  execve(cmd.c_str(), data, environ);
	  destroy(argv);
	  perror("execve");
	  exit(EXIT_FAILURE);
	}else{ // else search PATH variable for command
	  std::pair<std::vector<char*>, int> env_props = get_env_fp();
	  std::string full_cmd = find_cmd(env_props.first, cmd, env_props.second);
	  close_pipes(pipes);
	  destroy(env_props.first);
	  execve(full_cmd.c_str(), data, environ);
	  perror("execve");
	  destroy(argv);
	  exit(EXIT_FAILURE);
	}
      }
     return 0;
  }

  /*--------------------------------Parsing Input------------------*/
  //replace any shell variables:
  std::string replaceVars(std::string & arg, bool set){
       size_t pos = arg.find("$");
       //if $ found
       if(pos != std::string::npos){
	 std::string initial = arg.substr(0,pos);
	 std::string varname = arg.substr(pos+1);
	 std::string behind = "";
	 size_t pos_behind = varname.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_");
	 
	 if(pos_behind != std::string::npos){
	   behind = varname.substr(pos_behind);
	   behind = replaceVars(behind,false);
	   varname = varname.substr(0, pos_behind);
	 }

	 if(shellVars.find(varname) != shellVars.end()){
	   varname = shellVars.at(varname);
	 }else{	    
	   if(!set){
	     varname = "";
	   }else{
	     varname = "$"+varname;
	   }
	  } 
	 std::string expanded = initial+varname+behind;
	 return expanded;
       }else{ // other return the same
	 return arg;
       }
     }
    
    
  // parses command line arguments, 
  //returns: string command (first argument), int argc, and vector of arguments
      
  std::pair<std::string,std::pair<int,std::vector<char *> > > parseArguments(std::string & line){
	//string curr = line;

        std::string cmd;
	std::vector<char *> args; 

	std::string curr_arg;
	bool escape_seq = false;
	bool shell_var = false;
	int arg_count =0;

 
	for(size_t i =0; i<line.size(); i++){
	  if(line[i]== '\\' && !escape_seq){ // deal with escape characters
	    escape_seq=true;
	    continue;
	  }
	  if(shell_var == true && arg_count==2){
	    if(line[i] == ' ' && curr_arg.size() <1 && escape_seq==false){
	      //skip leading space
	    }else{
	      curr_arg+=line[i];
	      if(line[i]!=' '){
		escape_seq=false;
	      }
	    }
	  }
	  else if(line[i]== ' ' && escape_seq==false){ //argument is a space and not in an escaped sequence
	    if(curr_arg.size() > 0){
	      if(arg_count == 0){ //first command
		cmd = curr_arg.c_str();
		if(cmd == "set"){
		  shell_var = true;
		}
	      }
	      if(cmd.compare("set") ==0 && arg_count ==1){
	        curr_arg=replaceVars(curr_arg,true);
	      }else{
		 curr_arg=replaceVars(curr_arg,false);
	      } 
              args.push_back(const_cast<char*>(strdup(curr_arg.c_str())));
	      curr_arg = "";
	      arg_count ++;
	    }
	  }else{
	    curr_arg += line[i];
	    if(line[i] != ' '){
	      escape_seq = false;
	    }
	  }
	}
  
	if(curr_arg.size() > 0){
	  if(arg_count ==0){
	    cmd = curr_arg;
	  }
	  if(cmd.compare("set") ==0 && arg_count ==1){
	        curr_arg=replaceVars(curr_arg,true);
	  }else{
	        curr_arg=replaceVars(curr_arg,false);
	  } 
	  
	  args.push_back(const_cast<char*>(strdup((curr_arg.c_str()))));
	  arg_count ++;
	}
        return make_pair(cmd.c_str(),make_pair(arg_count,args));
   }

  
  /*-------------- User Interface/Interaction ----------------*/
       
      //prints the command prompt,
      //uses getcwd command to get current working directory for prompt
      void printPrompt(){
	char path[PATH_MAX+1];
	if(getcwd(path,sizeof(path)) ==NULL){
	  perror("can't get current working directory\n");
	  exit(EXIT_FAILURE);
	}else{
	  std::cout << "myShell:"<< path << " $";
	}
      }


 public:
      //read user input, start entire process
      int readInput(){
	loadEnv2Shell();
	std::string line="";
	std::pair<std::string, std::pair<int,std::vector <char *> > > argv;

	do{ // use a do while loop to read input after the prompt  
	  argv = parseArguments(line);
	  if(argv.first.size() > 0){
	    if(argv.first != "exit"){
	      int argc =argv.second.first;
	      runMulti(argv.first, argv.second.second,argc);
	    }else{
	      destroy(argv.second.second);
	      break;
	    }
	  }
	  printPrompt();
	}while(getline(std::cin, line));
       
	return 0;
      }
};
#endif
