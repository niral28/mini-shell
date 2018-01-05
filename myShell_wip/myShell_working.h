#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <map>

using namespace std;
extern char ** environ;

class MyShell{
 private:
  map<string, string> shellVars;

 public:
  MyShell(){

  }
  ~MyShell(){
    shellVars.clear();
  }

  //check if variable name is valid:
  bool validateVarName(string varname){
    cout <<"validating:"<< varname << "\n";
    size_t pos = varname.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_");
    if(pos ==string::npos){
      //      cout << "valid name"<<"\n";
      return true;
    }
    //cout << varname << " :"<<pos<<"\n";
    return false;
  }
  
  //set shell variables (only persist for existence of shell
  int setShellVar(string & cmd, vector<char*> & argv){
    if(argv.size() !=3){
      cerr << "incorrect arg size:" << argv.size() << "\n";
    }
    string varname = argv[1];
    cout << "varname:"<<varname<<"\n";
    if(!validateVarName(varname)){
      cout << "invalid variable name!\n";
      return -1;
    }else{
      shellVars[varname] = argv[2];
    }
    return 0;
      //cout << shellVars[argv[1]] << "\n";
  }

  //load environmental variables into the shell:
  void loadEnv2Shell(){
    int i =0;
    char * curr = environ[i];
    while(curr !=NULL){
      string var = string(curr);
      string key = "";
      string val = "";
      size_t pos = var.find("=");
      if(pos!= string::npos){
	key =  var.substr(0,pos);
	val = var.substr(pos+1);
	shellVars[key] = val;
      }
      i++;
      curr = environ[i];
    }
  }

  //only updates when export is called:
  void exportVar(string key){
    if(shellVars.find(key) != shellVars.end()){
       int i = setenv(key.c_str(), shellVars[key].c_str(),1);
       if(i!=0){
	 cerr<<"unable to export variable:" << key << "\n";
       }
    }else{
      cerr << "variable:" << key << " does not exist\n";
    }
  }
  
  //get PATH environment filepaths:
  pair<vector<char *>,int> get_env_fp(){
    vector<char*> env;
    char * path = getenv("PATH");
    string curr_env = "";
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
    //cout << argc << "\n";
    return make_pair(env,argc);
  } //end

  
  //search path directory for command
  string find_cmd(vector<char *> & env, string & cmd, int argc){
  
    for(int i=0; i<argc; i++){
      string curr_fp = env.at(i);
      curr_fp+= "/"+ cmd;
      ifstream file(curr_fp.c_str());
      if(file.good()){
	//cout << "FOUND:" << curr_fp << "\n";
	return curr_fp;
      }
    }
    cerr << "Command " << cmd << " not found\n";
    exit(EXIT_FAILURE);
  } //end

  //method implements cd command:
  int changeDir(string & path){
    if(path == "~"){
      path = getenv("HOME");
    }
    int i = chdir(path.c_str());
    switch(i) {
    case EACCES:
      perror("Search permission denied\n");
      return i;
    case EFAULT:
      perror("Path points outside your accesible address space\n");
      return i;
    case ENOENT:
      perror("The directory specified in path does not exist\n");
      return i;
    case 0:
      
      return 0;
    default:
      perror("failed\n");
      return i;
    }
  } 

  //commands that are built into the shell (no need to fork)
  int builtInCommands(string & cmd, vector<char*> & argv, int argc){
    int retval = 0;
    if(cmd.compare("cd") == 0){
    //execute cd stuff
      if(argc < 2){
	cout << "only 1 argument specified\n";
	retval = -1;
      }else{
	string path = argv[1];
	retval= changeDir(path);
      }
    }else if(cmd.compare("set") ==0){ //set variables	
      if(argc >= 3){
	cout << "here\n";
	retval=setShellVar(cmd, argv);
      }else{
	cerr << "not enough arguments,"<<argc<<" given,3 required\n";
	retval = -1;
      }
    }else if(cmd.compare("export") == 0){
      if(argc !=2){
	cerr << "not enough arguments, expect 2," << argc <<" given\n";
	retval = -1;
      }else{
	string key= string(argv[1]);
	exportVar(key);
      }
    }
    destroy(argv);
    return retval;
  }

  
  //actually execute command:
  int runCommand(string & cmd, vector<char *> & argv, int argc){
    pid_t cpid,w;
    int wstatus;
    char ** data= NULL;
    //built-in commands section (don't fork):
    if(cmd.compare("cd") == 0 || cmd.compare("set") == 0 || cmd.compare("export") == 0){
        return builtInCommands(cmd, argv,argc);
    }else{ // non built-in functions
	  //cite: code below modified from waitpid manpage
	  cpid = fork();
	  if(cpid == -1){
	    perror("fork");
	    exit(EXIT_FAILURE);
	  }
	  //child pid stuff:
	  if(cpid ==0){ 
	    if(cmd.size() <1){
	      cerr << "not enough arguments\n";
	      exit(EXIT_FAILURE);
	    }else if(cmd.compare("exit") ==0){
	      exit(EXIT_SUCCESS);
	    }
	    //convert command line arguments to char *
	    data = new char *[argc+1];
	    for(int i=0; i<argc; i++){
	      data[i] = argv.at(i);
	    }
	    data[argc] = NULL;
	    
	    //if path specified in command, let it run
	    if(cmd.find("/") != string::npos){
	      execve(cmd.c_str(), data, environ);
	      destroy(argv);
	      perror("execve");
	      exit(EXIT_FAILURE);
	    }else{ // else search path variable for command
	      pair<vector<char*>, int> env_props = get_env_fp();
	      string full_cmd = find_cmd(env_props.first, cmd, env_props.second);
	      execve(full_cmd.c_str(), data, environ);
	      perror("execve");
	      destroy(argv);
	      exit(EXIT_FAILURE);
	    }
	  }else{ // parent pid stuff:
	    do{
	      w = waitpid(cpid,&wstatus, WUNTRACED | WCONTINUED);
	      if(w == -1){
		perror("waitpid");
		exit(EXIT_FAILURE);
	      }
	      if(WIFEXITED(wstatus)){
		if(cmd.compare("cd")==0 && argc >=2){
        	  string path = argv[1];
		  changeDir(path);
		}
		cerr << "Program exited with status " << WEXITSTATUS(wstatus)<<"\n" ;
	      }else if(WIFSIGNALED(wstatus)){
		cerr<< "Program was killed by signal " <<WTERMSIG(wstatus)<<"\n" ; 
	      }
	    }while(!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
	    destroy(argv);
	  return wstatus;
	}
    }
	return 0;
  }

     //replace any variables:
  string replaceVars(string & arg, bool set){
       size_t pos = arg.find("$");
       //if $ found
       if(pos != string::npos){
	 string initial = arg.substr(0,pos);
	 string varname = arg.substr(pos+1);
	 string behind = "";
	 size_t pos_behind = varname.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_");
	 
	 if(pos_behind != string::npos){
	   behind = varname.substr(pos_behind);
	   cout <<"behind:"<< behind << "\n";
	   behind = replaceVars(behind,false);
	   cout <<"behind replaced:"<< behind << "\n";
	   varname = varname.substr(0, pos_behind);
	 }

	 if(shellVars.find(varname) != shellVars.end()){
	   varname = shellVars.at(varname);
	 }else{	    
	   //cout << "variable not found\n";
	   if(!set){
	     varname = "";
	   }else{
	     varname = "$"+varname;
	   }
	  } 
	 string expanded = initial+varname+behind;
	 //cout << "expanded:"<< expanded << "\n";
	 return expanded;
       }else{ // other return the same
	 return arg;
       }
     }
    
      /* 
	 parses command line arguments, 
	 returns: string command (first argument), int argc, and vector of arguments
      */
      pair<string,pair<int,vector<char *>>> parseArguments(string & line){
	//string curr = line;

	string cmd;
	vector<char *> args; 

	string curr_arg;
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

      /*
	prints the command prompt,
	uses getcwd command to get current working directory for prompt
      */
      void printPrompt(){
	char path[PATH_MAX+1];
	if(getcwd(path,sizeof(path)) ==NULL){
	  perror("can't get current working directory\n");
	  exit(EXIT_FAILURE);
	}else{
	  cout << "myShell:"<< path << " $";
	}
      }

      /*
	read's user input 
      */
      void readInput(){
	loadEnv2Shell();
	string line="";
	pair<string, pair<int,vector <char *>>> argv;
	do{
	  argv = parseArguments(line);

	  if(argv.first.size() > 0){
	    if(argv.first.compare("exit")!=0){
	      //std::cout << argv.first<<"\n";
	      int argc =argv.second.first;
	      runCommand(argv.first, argv.second.second,argc);
	    }else{
	      exit(EXIT_SUCCESS);
	    }
	  }
	  printPrompt();
	}while(getline(cin, line));
      }

      //need time to think about this one:
      void destroy(vector<char *> & args){
      	for(size_t i = 0; i<args.size(); i++){
      	  free(args.at(i));
	}
	/* if(*data !=NULL){ */
	/*   char * curr = *data; */
	/*   size_t i = 0; */
	/*   while(curr !=NULL){ */
	/*     delete curr; */
	/*     i++; */
	/*     curr = data[i]; */
	/*   } */
	/* } */
      }

   };
