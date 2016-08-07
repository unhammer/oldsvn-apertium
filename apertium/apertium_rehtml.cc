#include <iostream>
#include <string>
#include <stack>
#include <stdio.h>
#include <map>
#include <fstream>
#include <utility>
#include <sqlite3.h>
#include <stdlib.h>
#include <sstream>


#include <cstdio>
#include <cstdlib>
#include <libgen.h>
#include <string>
#include <getopt.h>
using namespace std;

map<int,string> opening_tags;
map<int, string> closing_tags;

void make_maps(string filename)
{ 
  sqlite3 *db;
  sqlite3_open(filename.c_str(), & db);
  string selectQuery = "SELECT * FROM TAGS_DATA;";
    sqlite3_stmt *selectStmt;
  
    if(sqlite3_prepare_v2(db, "select * from TAGS_DATA;", -1, &selectStmt, 0) != SQLITE_OK) cout << "Didn't Select Item!" << endl;
    else
    {
      int columns = sqlite3_column_count(selectStmt);
      int result = 0;
      while(true)
      {
          result = sqlite3_step(selectStmt);
          if(result == SQLITE_ROW)
          {
              
              stringstream s;
              s << sqlite3_column_text(selectStmt, 0);
              int id = atoi(s.str().c_str());

              stringstream o_tag;
              string opening_tag;
              o_tag << sqlite3_column_text(selectStmt, 1);
              opening_tag = o_tag.str();
          
        stringstream c_tag;
              string closing_tag;
              c_tag << sqlite3_column_text(selectStmt, 2);
              closing_tag = c_tag.str();

              opening_tags[id] = opening_tag;
              closing_tags[id] = closing_tag;

          }
          else
              break;
      }
      sqlite3_finalize(selectStmt);
    } 
}

bool is_blank(char ch)
{
  if(ch == ' ' || ch == '\n' || ch == '\t' || ch == '\b' || ch == '\f')
    return true;
  else
    return false;
}


void empty_stack(stack<int> &mystack)
{
  stack<int> temp;
  while(!mystack.empty())
  {
    temp.push(mystack.top());
    mystack.pop();
  }
  while(!temp.empty())
  { 
    //cout << "Closing tag with id ="<< temp.top() << endl;
    cout << closing_tags[temp.top()];
    temp.pop();
  }
}

void usage(char *progname)
{
  wcerr << L"USAGE: " << basename(progname) << L" [input_file [output_file]]" << endl;
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{ 
  string str;
  make_maps("tags_data.db");

  if((argc-optind+1) > 3)
  {
    usage(argv[0]);
  }

  FILE *input, *output;
  
  if((argc-optind+1) == 1)
  {
    input = stdin;
    output = stdout;
    char mystring [100];
    FILE *fp;
    fp = fopen("input.xml","w+b");
    while(fgets(mystring,100,stdin) != NULL)
    {
      fputs(mystring,fp);
    }
    fclose(fp); 
  ifstream in("input.xml");
  std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  str = s;

  }
  else if ((argc-optind+1) == 2)
  {
    input = fopen(argv[argc-1], "r");
    if(!input)
    {
      usage(argv[0]);
    }
    output = stdout;
  
    if(feof(input))
    {
      wcerr << L"ERROR: Can't read file '" << argv[1] << L"'" << endl;
      exit(EXIT_FAILURE);
    }
    ifstream in(argv[argc-1]);
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    str = s;
  }
  else
  {
    input = fopen(argv[argc-2], "r");
    output = fopen(argv[argc-1], "w+b");

    if(!input || !output)
    {
      usage(argv[0]);
    }
    if(feof(input))
    {
      wcerr << L"ERROR: Can't read file '" << argv[1] << L"'" << endl;
      exit(EXIT_FAILURE);
    }
    ifstream in(argv[argc-2]);
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    str = s;
  }


#ifdef _MSC_VER
    _setmode(_fileno(input), _O_U8TEXT);
    _setmode(_fileno(output), _O_U8TEXT);
#endif

  
  int l = str.length();
  stack<int> mystack;
  int i = 0;
  while(i<l)
  { 
    if(((str[i] == '[' && i == 0) || (str[i] == '[' && str[i-1] != '\\')) && i+1 < l)
    { 
      empty_stack(mystack);
      if(str[i+1]=='{' && i+1 < l)
      { 
        int j = i+2;
        while(str[j] != '}')
        { 
          string tag;
          while(str[j]!=',' && str[j]!='}')
            tag += str[j++];
          //cout << "Tag is" << tag << "this\n";
          
          int ind = atoi(tag.c_str());

          mystack.push(ind);
          
          if(str[j] == '}')
            break;
          else
            j++;
        }
        
        stack<int> temp = mystack;
        while(!temp.empty())
        { 
          cout << opening_tags[temp.top()];
          temp.pop();
        }
        i = j+2;
      }
      else
      { 
        int j = i+1;
        string tag;
        bool blank_super = true;
        while(str[j] != ']' && j < l)
        { 
          if(!is_blank(str[j]))
            blank_super = false;
          tag += str[j++];
        }
        if(blank_super)
        { 
          cout << tag;
        }
        else
        { 
          int ind = atoi(tag.c_str());
          cout << opening_tags[ind];
        }
        i = j+1;
      }
    }
    else
    {
      if(str[i] == '\\')
        i++;
      else
        cout << str[i++];
    }
  }

  cout << endl;
  return 0;
}
