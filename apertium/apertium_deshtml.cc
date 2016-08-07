#include <stack>
#include <vector>
#include <map>
#include <cstring> 
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>

#include <cstdio>
#include <cstdlib>
#include <libgen.h>
#include <string>
#include <getopt.h>

// #include <lttoolbox/lt_locale.h>
// #include "apertium_config.h"
// #include <apertium/unlocked_cstdio.h>

#ifdef _MSC_VER
#include <io.h>
#include <fcntl.h>
#endif
// #include <apertium/string_utils.h>

// using namespace Apertium;
using namespace std;


#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()
using namespace std;

int indices = 1;

stack <xmlNode*> vec;
map<string, int> TAGS;
bool in_inline = false;
bool in_superblank = false;


void usage(char *progname)
{
  wcerr << L"USAGE: " << basename(progname) << L" [input_file [output_file]]" << endl;
  exit(EXIT_FAILURE);
}


int is_inline(char *tag) {
	if(TAGS.find(tag) != TAGS.end()) {
		return 1;
	}
	return 0;
}
map <int, string > closing_tags;
map <int, string > opening_tags;

void init_tags() {
	TAGS["em"] = 1;
	TAGS["u"] = 1;
	TAGS["i"] = 1;
	TAGS["b"] = 1;
	TAGS["span"] = 1;
	TAGS["a"] = 1;
	TAGS["abbr"] = 1;
	TAGS["bdo"] = 1;
	TAGS["big"] = 1;
	TAGS["cite"] = 1;
	TAGS["code"] = 1;
	TAGS["dfn"] = 1;
	TAGS["font"] = 1;
	TAGS["img"] = 1;
	TAGS["input"] = 1;
	TAGS["kbd"] = 1;
	TAGS["label"] = 1;
	TAGS["q"] = 1;
	TAGS["s"] = 1;
	TAGS["samp"] = 1;
	TAGS["select"] = 1;
	TAGS["small"] = 1;
	TAGS["strike"] = 1;
	TAGS["strong"] = 1;
	TAGS["sub"] = 1;
	TAGS["sup"] = 1;
	TAGS["textarea"] = 1;
	TAGS["tt"] = 1;
	TAGS["u"] = 1;
	TAGS["var"] = 1;
}

bool is_blank(char ch)
{
	if(ch == ' ' || ch == '\n' || ch == '\t')
		return true;
	else
		return false;
}

void print_stack(ostream& outfile, string &start_blank) {
	if(!vec.size())
		return;
	if(start_blank.length())
		outfile << "[" << start_blank << "]";
	outfile<<"[{";
	int comma = 0;
	for (std::stack<xmlNode*> dump = vec; !dump.empty(); dump.pop())
	{	
		stringstream opening_temp_tag;
		xmlAttr *cur_attr = NULL;
    	xmlChar *attr;
    	xmlNode *cur_node = dump.top();
    	if(comma)
			outfile <<"," << indices;
		else
			outfile << indices;
		comma = 1;
		opening_temp_tag << "<" << cur_node->name; 
		
    	for (cur_attr = cur_node->properties; cur_attr; cur_attr = cur_attr->next) 
		{

   			opening_temp_tag << " " << cur_attr->name << "=";
   			attr =  xmlNodeGetContent((xmlNode*)cur_attr);

  			opening_temp_tag << "\"" << attr << "\"";
		}
		opening_temp_tag << ">";
		opening_tags[indices] = opening_temp_tag.str();
		stringstream closing_temp_tag;
		closing_temp_tag << "</" << cur_node->name << ">";
		closing_tags[indices] = closing_temp_tag.str();
		indices++;
	}
	outfile << "}]";
}

void print_element_names(xmlNode * a_node, ostream& outfile) {

	if(!a_node)
	{
		if(in_superblank)
		{
			in_superblank = false;
			outfile << "]";
		}
	}

	xmlNode *cur_node = NULL;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {

			if (is_inline((char*)cur_node->name))
			{	
				if(in_superblank)
				{
					outfile << "]";
					in_superblank = false;
				}
				vec.push((xmlNode*)cur_node);
				// in_inline = true;
				in_superblank = false;
			}
			else
			{	
				if(!in_superblank)
				{
					outfile << "[";
					in_superblank = true;
				}
				xmlAttr *cur_attr = NULL;
    			xmlChar *attr;
    			// printf("[<%s%d",cur_node->name,indices);
    			outfile << "<" << cur_node->name;
    			//attributes << indices << "=<" << cur_node->name;

    			for (cur_attr = cur_node->properties; cur_attr; cur_attr = cur_attr->next) 
    			{
    				outfile << " " << cur_attr->name << " = ";
	       			attr =  xmlNodeGetContent((xmlNode*)cur_attr);
	      			outfile << "\"" << attr << "\"";
	    		}
	    		outfile << ">";
			}
			print_element_names(cur_node->children,outfile);
			if (is_inline((char*)cur_node->name))
			{	
				outfile << "[]";
				vec.pop();

			}
			else
			{	
				outfile << "[</" << cur_node->name << ">]";
			}
		}
		else 
		{	

			char* strng;
			strng = (char*)cur_node->content;
			// cout << "String part is" << strng << "this" << endl;
			// if(in_superblank && !strng)
			// 	outfile << "]";
			int l = strlen(strng);

			int num = 0;
			while(is_blank(strng[num]))
				num++;
			//outfile << num;
			if(num < l)
			{	
				if(in_superblank)
				{
					in_superblank = false;
					outfile << "]";
				}
				int k = 0;
				//print_stack(outfile,start_blank);

				string start_blank="";
				// while(is_blank(strng[k]) && k < l)
				// 	start_blank = strng[k++];

				// if(start_blank.length())
				// 	outfile << "[" << start_blank << "]";
				print_stack(outfile,start_blank);
				int start_index = k;

				k = l-1;
				while(k>=0 && is_blank(strng[k]))
					k--;
				string end_blank ="";
				int end_index = k;
				k++;
				while(k<l)
					end_blank += strng[k++];

				k = start_index;
				while(k+1 <= end_index)
				{	
					
					if(is_blank(strng[k]) && is_blank(strng[k+1]))
					{	
						string buf="";
						int t = k;
						while(t < l)
						{	if(is_blank(strng[t]))
							{
								buf += strng[t];
								t++;
							}
							else
								break;
						}
				
						outfile << "[" << buf << "]";
						k = t;
					}
					else
					{	
						if(strng[k] == '[' || strng[k] == ']')
							outfile << "\\" << strng[k];
						else
							outfile << strng[k];
						k++;
					}
				}
				if(k == end_index)
				{	
					if(strng[k] == '[' || strng[k] == ']')
						outfile << "\\" << strng[k];
					else
						outfile << strng[k];
				}
				if(end_blank.length())
				{	
					// cout << "End blank is" << end_blank <<"this" << endl;
					outfile << "[" << end_blank << "]";
				}
			}
			else
			{	
				if(!in_superblank)
					outfile << "[" << strng << "]";
				else
					outfile << strng;
			}
		}
	}
}



string merge_blocks(string s)
{		
	int l = s.length();
	int i = 1;
	string ans="";
	ans += s[0];
	// cout << s << endl << endl;
	//outputfile << s << endl << endl;
	bool block;

	while(i<l)
	{	
		if(i+2 > l)
			break;
		if((s[i]==']' && s[i+1]=='[' && s[i+2]!='{' && s[i-1]!='}' && s[i-1]!= '\\'))
			i+=2;
		ans += s[i];
		i++;
	}
	return ans;
}
	
string remove_repeatition_superblanks(string s)
{
	int i = 0;
	string ans = "";
	int l = s.length();
	while(i < l)
	{
		if((i == 0 && s[i] == '[' && s[i+1] == ']') || (i > 0 && s[i-1] != '\\' && s[i] == '[' && s[i+1] == ']'))
		{	
			string buf = "[]";
			int j = i+2;
			int non_super_found = 1;
			while(true)
			{
				if(s[j] != '[')
				{
					non_super_found = 0;
					break;
				}
				else	
					break;
			
				j++;
			}
			if(!non_super_found)
				ans += buf;
			i = j;
		}
		else
		{
			ans += s[i++];
		}
	}
	return ans;
}


string add_non_inline_as_ids(string s)
{	
	int i = 0;
	int l = s.length();
	string ans="";
	while(i+1 < l)
	{
		if(s[i]=='[' && s[i+1]!='{' && s[i-1] != '\\')
		{
			int j = i+1;
			bool just_blank = true;
			string buf="";
			while(j < l && s[j]!=']')
			{
				if(!is_blank(s[j]))
					just_blank = false;
				buf += s[j++];
			}
			if(!just_blank)
			{
				ans += "[" + SSTR(indices) + "]";
				opening_tags[indices] = buf;
				closing_tags[indices] = "";
				indices++;
			}
			else
			{
				ans += "[" + buf + "]";
			}
			i = j+1;
		}
		else
			ans += s[i++];
	}
	return ans;
}

void put_in_database(string filename)
{	
    ofstream attributes (filename.c_str());
	if(!attributes.is_open())
	{
		cout << "Unable to open attributes file\n";
	}
	for(int i = 1; i < indices; i++)
	{	
		// cout << i << "=" << opening_tags[i] << "," << closing_tags[i] + '\0';
		attributes << i << "=" << opening_tags[i];
		if(closing_tags[i].length())
			attributes << "," << closing_tags[i] + '\0';
		else
			attributes << '\0';
	}
}




int main(int argc, char **argv)
{	

	ofstream outfile ("temp.txt");
	
	init_tags();
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;

	// if (argc != 2)
	// 	return(1);
	LIBXML_TEST_VERSION

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
    doc = xmlReadFile("input.xml", NULL, 0);

	if (doc == NULL) {
		printf("error: could not parse file %s\n", argv[1]);
	}

  }
  else if ((argc-optind+1) == 2)
  {
    input = fopen(argv[argc-1], "r");
    if(!input)
    {
      usage(argv[0]);
    }
    output = stdout;
    doc = xmlReadFile(argv[argc-1],NULL,0);
	if (doc == NULL) {
		printf("error: could not parse file %s\n", argv[1]);
	}
  }
  else
  {
    input = fopen(argv[argc-2], "r");
    output = fopen(argv[argc-1], "w");

    if(!input || !output)
    {
      usage(argv[0]);
    }
    doc = xmlReadFile(argv[argc-2],NULL,0);
	if (doc == NULL) {
		printf("error: could not parse file %s\n", argv[1]);
	}
  }

  if(feof(input))
  {
    wcerr << L"ERROR: Can't read file '" << argv[1] << L"'" << endl;
    exit(EXIT_FAILURE);
  }

#ifdef _MSC_VER
    _setmode(_fileno(input), _O_U8TEXT);
    _setmode(_fileno(output), _O_U8TEXT);
#endif


	LIBXML_TEST_VERSION

	// doc = xmlReadFile("input.xml", NULL, 0);

	if (doc == NULL) {
		printf("error: could not parse file %s\n", argv[1]);
	}

	root_element = xmlDocGetRootElement(doc);
	// cerr << "Parsing starts\n";

	print_element_names(root_element,outfile);
	outfile<<endl;

	xmlFreeDoc(doc);

	xmlCleanupParser();

	ifstream in("temp.txt");
	string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		// cout << s << endl;

	string rds = remove_repeatition_superblanks(s);
	// cout << rds << endl << endl;

	string merged = merge_blocks(rds);
	// cout << merged << endl << endl;
	
	string final = add_non_inline_as_ids(merged);
	// cout << final << endl;
	// ofstream deformatted_output (stdout);
	// deformatted_output << final;
	fputs(final.c_str(),output);
	fputs("\n",output);
	//print_maps();
	string filename = "tags_data.txt";
	put_in_database(filename);

	// ifstream tags("tags_data.txt");
	// string st((std::istreambuf_iterator<char>(tags)), std::istreambuf_iterator<char>());
	// cout << st.length();

	return 0;
}
