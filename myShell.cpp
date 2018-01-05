#include <cstdlib>
#include <string>
#include <iostream>
#include "myShell.h"
#include <vector>


int main(int argc, char ** argv){
  // readInput(argc,argv);

  // int i =0;
  // char * curr = environ[i];
  // while(curr != NULL){
  //   cout << curr << "\n";
  //   i++;
  //   curr = environ[i];
  //  }
  MyShell  mp;
  //std::cout << "\n";
  return mp.readInput();
   //delete mp;
}



