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

#include <tidy.h>
#include <tidybuffio.h>

using namespace std;

map<int,string> opening_tags;
map<int, string> closing_tags;

void make_maps(string filename)
{		
	ifstream tags(filename.c_str());
	string str((std::istreambuf_iterator<char>(tags)), std::istreambuf_iterator<char>());
	int l = str.length();
	string opening_tag = "";
	string closing_tag = "";
	string s_index;
	bool left = true;
	int i = 0;
	int index;
	// outfile << str << endl;
	while(i < l)
	{	
		if(left)
		{	
			s_index = "";
			index = -1;
			while(str[i]!='=')
				s_index += str[i++];
			left = false;
			i++;
			index = atoi(s_index.c_str());
			// outfile << "index is:" << index << endl;
		}
		else
		{	
			opening_tag = "";
			closing_tag = "";
			while(str[i]!='\0')
			{	
				if(str[i]==',')
				{
					i++;
					while(str[i]!='\0')
						closing_tag += str[i++];
				}
				else
					opening_tag += str[i++];
			}
			i++;
			opening_tags[index] = opening_tag;
			closing_tags[index] = closing_tag;
			left = true;
		}
	}
}

bool is_blank(char ch)
{
	if(ch == ' ' || ch == '\n' || ch == '\t' || ch == '\b' || ch == '\f')
		return true;
	else
		return false;
}


void empty_stack(stack<int> &mystack, ofstream &outfile)
{
	stack<int> temp;
	while(!mystack.empty())
	{
		temp.push(mystack.top());
		mystack.pop();
	}
	while(!temp.empty())
	{	
		outfile << closing_tags[temp.top()];
		temp.pop();
	}
}

string cleanhtml(const std::string &html)
{
    // init a tidy document
    TidyDoc tidy_doc=tidyCreate();
    TidyBuffer output_buffer= {0};
 
    // configure tidy
    // the flags tell tidy to output xml and disable warnings
    bool config_success=tidyOptSetBool(tidy_doc,TidyXmlOut,yes)
                        && tidyOptSetBool(tidy_doc,TidyQuiet,yes)
                        && tidyOptSetBool(tidy_doc,TidyNumEntities,yes)
                        && tidyOptSetBool(tidy_doc,TidyShowWarnings,no);
 
    int tidy_rescode=-1;
 
    // parse input
    if(config_success)
        tidy_rescode=tidyParseString(tidy_doc,html.c_str());
 
    // process html
    if(tidy_rescode>=0)
        tidy_rescode=tidySaveBuffer(tidy_doc,&output_buffer);
 
    if(tidy_rescode<0)
        throw("tidy has a error: "+tidy_rescode);
 
    std::string result=(char *)output_buffer.bp;
    tidyBufFree(&output_buffer);
    tidyRelease(tidy_doc);
 
    return result;
}



void usage(char *progname)
{
  wcerr << L"USAGE: " << basename(progname) << L" [input_file [output_file]]" << endl;
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{	
	ofstream outfile ("reformated.txt");
	string str;
	make_maps("tags_data.txt");

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
			empty_stack(mystack,outfile);
			if(str[i+1]=='{' && i+1 < l)
			{	
				int j = i+2;
				while(str[j] != '}')
				{	
					string tag;
					while(str[j]!=',' && str[j]!='}')
						tag += str[j++];
					
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
					outfile << opening_tags[temp.top()];
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
					outfile << tag;
				}
				else
				{	
					int ind = atoi(tag.c_str());
					outfile << opening_tags[ind];
				}
				i = j+1;
			}
		}
		else
		{
			if(str[i] == '\\')
				i++;
			else
				outfile << str[i++];
		}
	}

	outfile << endl;
	ifstream in("reformated.txt");
	string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	s = cleanhtml(s);
	fputs(s.c_str(),output);
	fclose(output);

	return 0;
}
