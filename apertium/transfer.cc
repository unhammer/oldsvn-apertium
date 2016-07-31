/*
 * Copyright (C) 2005--2015 Universitat d'Alacant / Universidad de Alicante
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include <apertium/transfer.h>
#include <apertium/trx_reader.h>
#include <apertium/utf_converter.h>
#include <apertium/string_utils.h>
#include <lttoolbox/compression.h>
#include <lttoolbox/xml_parse_util.h>
#include <pcre.h>

#include <cctype>
#include <iostream>
#include <stack>
#include <cerrno>

using namespace Apertium;
using namespace std;

void
Transfer::copy(Transfer const &o)
{
}

void
Transfer::destroy()
{
  if(me)
  {
    delete me;
    me = NULL;
  }
  if(doc)
  {
    xmlFreeDoc(doc);
    doc = NULL;
  }  
}

Transfer::Transfer()
{
  me = NULL;
  doc = NULL;
  root_element = NULL;
  lastrule = NULL;
  defaultAttrs = lu;
  useBilingual = true;
  preBilingual = false;
  isExtended = false;
  null_flush = false;
  internal_null_flush = false;
  trace = false;
  emptyblank = "";
}

Transfer::~Transfer()
{
  destroy();
}

Transfer::Transfer(Transfer const &o)
{
  copy(o);
}

Transfer &
Transfer::operator =(Transfer const &o)
{
  if(this != &o)
  {
    destroy();
    copy(o);
  }
  return *this;
}

void 
Transfer::readData(FILE *in)
{
  alphabet.read(in);
  any_char = alphabet(TRXReader::ANY_CHAR);
  any_tag = alphabet(TRXReader::ANY_TAG);

  Transducer t;
  t.read(in, alphabet.size());
  
  map<int, int> finals;  
  
  // finals
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    int key = Compression::multibyte_read(in);
    finals[key] = Compression::multibyte_read(in);
  }  
  
  me = new MatchExe(t, finals);
 
  // attr_items
  bool recompile_attrs = Compression::string_read(in) != string(pcre_version());
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    string const cad_k = UtfConverter::toUtf8(Compression::wstring_read(in));
    attr_items[cad_k].read(in);
    wstring fallback = Compression::wstring_read(in);
    if(recompile_attrs) {
      attr_items[cad_k].compile(UtfConverter::toUtf8(fallback));
    }
  }

  // variables
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    string const cad_k = UtfConverter::toUtf8(Compression::wstring_read(in));
    variables[cad_k] = UtfConverter::toUtf8(Compression::wstring_read(in));
  }

  // macros
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    string const cad_k = UtfConverter::toUtf8(Compression::wstring_read(in));
    macros[cad_k] = Compression::multibyte_read(in);
  }

  // lists
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    string const cad_k = UtfConverter::toUtf8(Compression::wstring_read(in));

    for(int j = 0, limit2 = Compression::multibyte_read(in); j != limit2; j++)
    {
      wstring const cad_v = Compression::wstring_read(in);
      lists[cad_k].insert(UtfConverter::toUtf8(cad_v));
      listslow[cad_k].insert(UtfConverter::toUtf8(StringUtils::tolower(cad_v)));
    }  
  }
}

void
Transfer::readBil(string const &fstfile)
{
  FILE *in = fopen(fstfile.c_str(), "rb");
  if(!in)
  {
    cerr << "Error: Could not open file '" << fstfile << "'." << endl;
    exit(EXIT_FAILURE);
  }
  fstp.load(in);
  fstp.initBiltrans();
  fclose(in);
}

void
Transfer::setExtendedDictionary(string const &fstfile)
{
  FILE *in = fopen(fstfile.c_str(), "rb");
  if(!in)
  {
    cerr << "Error: Could not open extended dictionary file '" << fstfile << "'." << endl;
    exit(EXIT_FAILURE);
  }
  extended.load(in);
  extended.initBiltrans();
  fclose(in);
  isExtended = true;
}

void
Transfer::read(string const &transferfile, string const &datafile,
	       string const &fstfile)
{
  readTransfer(transferfile);
  
  // datafile
  FILE *in = fopen(datafile.c_str(), "rb");
  if(!in)
  {
    cerr << "Error: Could not open file '" << datafile << "'." << endl;
    exit(EXIT_FAILURE);
  }
  readData(in);
  fclose(in);
  
  if(fstfile != "")
  {
    readBil(fstfile);
  }
}

void
Transfer::readTransfer(string const &in)
{
  doc = xmlReadFile(in.c_str(), NULL, 0);
  
  if(doc == NULL)
  {
    cerr << "Error: Could not parse file '" << in << "'." << endl;
    exit(EXIT_FAILURE);
  }
  
  root_element = xmlDocGetRootElement(doc);
  
  // search for root element attributes
  for(xmlAttr *i = root_element->properties; i != NULL; i = i->next)
  {
    if(!xmlStrcmp(i->name, (const xmlChar *) "default"))
    {
      if(!xmlStrcmp(i->children->content, (const xmlChar *) "chunk"))
      {
        defaultAttrs = chunk;
      }
      else
      {
        defaultAttrs = lu; // default value for 'default'
      }
    }  
  }
  
  // search for macros & rules
  for(xmlNode *i = root_element->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(!xmlStrcmp(i->name, (const xmlChar *) "section-def-macros"))
      {
        collectMacros(i);
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "section-rules"))
      {
        collectRules(i);
      }
    } 
  }
}

void
Transfer::collectRules(xmlNode *localroot)
{
  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      for(xmlNode *j = i->children; ; j = j->next)
      {
        if(j->type == XML_ELEMENT_NODE && !xmlStrcmp(j->name, (const xmlChar *) "action"))
        {
          rule_map.push_back(j);
          break;
        }
      }
    }
  }
}

void
Transfer::collectMacros(xmlNode *localroot)
{ 
  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {     
      macro_map.push_back(i);
    }
  }
}

bool
Transfer::checkIndex(xmlNode *element, int index, int limit)
{
  if(index >= limit)
  {
    wcerr << L"Error in " << UtfConverter::fromUtf8((char *) doc->URL) <<L": line " << element->line << endl;
    return false;
  }
  return true;
}


Transfer::best_blank_pos
Transfer::wordBlankPos(xmlNode *element, best_blank_pos best_so_far)
{ 
    wcerr << "In wordBlankPos\n";
  if (best_so_far.second == "lu" ||
       best_so_far.second == "whole" ||
       best_so_far.second == "lem" ||
       best_so_far.second == "lemh" ||
       best_so_far.second == "lemq") {
    return best_so_far;
  }
  map<xmlNode *, TransferInstr>::iterator it;
  it = evalStringCache.find(element);
  if(it == evalStringCache.end())
  {
    string _ = evalString(element).first;
    it = evalStringCache.find(element);
  }

  if(it != evalStringCache.end())
  {
    TransferInstr &ti = it->second;
    switch(ti.getType())
    {
      case ti_clip_sl:
      case ti_clip_tl:
      case ti_linkto_sl:
      case ti_linkto_tl:
        if(checkIndex(element, ti.getPos(), lword))
        {
          return std::make_pair<int, string>(ti.getPos(), ti.getContent());
        }
        break;

      default:
        return std::make_pair<int, string>(-1, NULL);
    }
  }
  else
  {
    wcerr << L"Warning: evalString didn't seem to fill the cache in " << UtfConverter::fromUtf8((char *) doc->URL) <<L": line " << element->line << endl;
  }
  return std::make_pair<int, string>(-1, NULL);
}


pair<string,int> 
Transfer::evalString(xmlNode *element)
{
  map<xmlNode *, TransferInstr>::iterator it;
  it = evalStringCache.find(element);
  if(it != evalStringCache.end())
  { 
    TransferInstr &ti = it->second;
    // wstring wstr (ti.getContent().begin(), ti.getContent().end());
    // wcout << "TransferInstr is " << wstr << " and position is " << ti.getPos() << endl;
    switch(ti.getType())
    {
      case ti_clip_sl:
        if(checkIndex(element, ti.getPos(), lword))
        {
          return make_pair(word[ti.getPos()]->source(attr_items[ti.getContent()], ti.getCondition()), ti.getPos());
        }
        break;

      case ti_clip_tl:
        if(checkIndex(element, ti.getPos(), lword))
        {
          // present = true;
          pair<string, int> p;
          p = make_pair(word[ti.getPos()]->target(attr_items[ti.getContent()], ti.getCondition()), ti.getPos());
          wstring wstr (p.first.begin(), p.first.end());
          // wcout << "ti_clip_tl is " << wstr << " and position is " << ti.getPos() << endl;
          if(p.first[0] != '<')
            present_words[p.first] = 1;
          // return p;
          return p;
        }
        break;
      
      case ti_linkto_sl:
        if(checkIndex(element, ti.getPos(), lword))
        {
          if(word[ti.getPos()]->source(attr_items[ti.getContent()], ti.getCondition()) != "")
          {
            return make_pair("<" + string((char *) ti.getPointer()) + ">", ti.getPos());
          }        
          else
          {
            return make_pair("",ti.getPos());
          }
        }
        break;
        
      case ti_linkto_tl:
        if(checkIndex(element, ti.getPos(), lword))
        {
          if(word[ti.getPos()]->target(attr_items[ti.getContent()], ti.getCondition()) != "")
          {
            return make_pair("<" + string((char *) ti.getPointer()) + ">", ti.getPos());
          }        
          else
          {
            return make_pair("", ti.getPos());
          }
        }
        break;
          
      case ti_var:
        return make_pair(variables[ti.getContent()], ti.getPos());

      case ti_lit_tag:
      case ti_lit:
        return make_pair(ti.getContent(), ti.getPos());
        
      case ti_b:
        // if(checkIndex(element, ti.getPos(), lblank))
        // {
        //   // wcout << "\nValue of ti.getPos() is " << ti.getPos() << endl;
        //   if(ti.getPos() >= 0)
        //   { 
        //     // wcout << "Value of ti.getPos() is " << ti.getPos() << endl;
        //     if(blank)
        //       return make_pair(*blank[ti.getPos()],ti.getPos());
        //     return make_pair("", ti.getPos());
        //     // return !blank?"":*(blank[ti.getPos()]);
        //   }
        //   return make_pair(" ", ti.getPos());
        // }
        // break;
        if(freeblank.empty()) {
          return make_pair(" ",-1);
        }
        else {
          wcerr << "In blanks\n";
          wstring blank = freeblank.front();
          freeblank.pop_front();
          wcerr <<L"blank=?"<<blank<<endl;
          if(blank.empty())     // TODO: or?
          {
            return make_pair(" ",-1);
          }
          else {
            wcerr <<L"blank="<<blank<<endl;
            return make_pair(UtfConverter::toUtf8(blank).c_str(),-1);
          }
        }
        
      case ti_get_case_from:
        if(checkIndex(element, ti.getPos(), lword))
        {
          return make_pair(copycase(word[ti.getPos()]->source(attr_items[ti.getContent()]),
                  evalString((xmlNode *) ti.getPointer()).first), ti.getPos());
        }
        break;
        
      case ti_case_of_sl:
        if(checkIndex(element, ti.getPos(), lword))
        {
          return make_pair(caseOf(word[ti.getPos()]->source(attr_items[ti.getContent()])), ti.getPos());
        }
        break;
      
      case ti_case_of_tl:
        if(checkIndex(element, ti.getPos(), lword))
        {
          return make_pair(caseOf(word[ti.getPos()]->target(attr_items[ti.getContent()])), ti.getPos());
        }
        break;
        
      default:
        return make_pair("",ti.getPos());
    }
    return make_pair("", ti.getPos());
  }

  if(!xmlStrcmp(element->name, (const xmlChar *) "clip"))
  {
    int pos = 0;
    xmlChar *part = NULL, *side = NULL, *as = NULL;
    bool queue = true;

    for(xmlAttr *i = element->properties; i != NULL; i = i->next)
    {
      if(!xmlStrcmp(i->name, (const xmlChar *) "side"))
      {
	side = i->children->content;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "part"))
      {
	part = i->children->content;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "pos"))
      {
	pos = atoi((const char *)i->children->content) - 1;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "queue"))
      {
        if(!xmlStrcmp(i->children->content, (const xmlChar *) "no"))
        {
          queue = false;
        }
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "link-to"))
      {
        as = i->children->content;
      }
    }

    if(as != NULL)
    {
      if(!xmlStrcmp(side, (const xmlChar *) "sl"))
      {
        evalStringCache[element] = TransferInstr(ti_linkto_sl, (const char *) part, pos, (void *) as, queue);
      }
      else
      {
        evalStringCache[element] = TransferInstr(ti_linkto_tl, (const char *) part, pos, (void *) as, queue);
      }      
    }      
    else if(!xmlStrcmp(side, (const xmlChar *) "sl"))
    {
      evalStringCache[element] = TransferInstr(ti_clip_sl, (const char *) part, pos, NULL, queue);
    }
    else
    {
      evalStringCache[element] = TransferInstr(ti_clip_tl, (const char *) part, pos, NULL, queue);
    }
  }
  else if(!xmlStrcmp(element->name, (const xmlChar *) "lit-tag"))
  {
    evalStringCache[element] = TransferInstr(ti_lit_tag, 
                                             tags((const char *) element->properties->children->content), 0);                                            
  }
  else if(!xmlStrcmp(element->name, (const xmlChar *) "lit"))
  {
    evalStringCache[element] = TransferInstr(ti_lit, string((char *) element->properties->children->content), 0);
  }
  else if(!xmlStrcmp(element->name, (const xmlChar *) "b"))
  {
    if(element->properties == NULL)
    {
      evalStringCache[element] = TransferInstr(ti_b, " ", -1);
    }
    else
    {
      int pos = atoi((const char *) element->properties->children->content) - 1;
      evalStringCache[element] = TransferInstr(ti_b, "", pos);
    }
  }
  else if(!xmlStrcmp(element->name, (const xmlChar *) "get-case-from"))
  {
    int pos = atoi((const char *) element->properties->children->content) - 1;
    xmlNode *param = NULL;
    for(xmlNode *i = element->children; i != NULL; i = i->next)
    {
      if(i->type == XML_ELEMENT_NODE)
      {
	param = i;
	break;
      }
    }

    evalStringCache[element] = TransferInstr(ti_get_case_from, "lem", pos, param);
  }
  else if(!xmlStrcmp(element->name, (const xmlChar *) "var"))
  {
    evalStringCache[element] = TransferInstr(ti_var, (const char *) element->properties->children->content, 0);
  }
  else if(!xmlStrcmp(element->name, (const xmlChar *) "case-of"))
  {
    int pos = 0;
    xmlChar *part = NULL, *side = NULL;

    for(xmlAttr *i = element->properties; i != NULL; i = i->next)
    {
      if(!xmlStrcmp(i->name, (const xmlChar *) "side"))
      {
	side = i->children->content;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "part"))
      {
	part = i->children->content;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "pos"))
      {
	pos = atoi((const char *) i->children->content) - 1;
      }
    }
      
    if(!xmlStrcmp(side, (const xmlChar *) "sl"))
    {
      evalStringCache[element] = TransferInstr(ti_case_of_sl, (const char *) part, pos);
    }
    else
    {
      evalStringCache[element] = TransferInstr(ti_case_of_tl, (const char *) part, pos);
    }    
  }
  else if(!xmlStrcmp(element->name, (const xmlChar *) "concat"))
  { 
    string value;
    for(xmlNode *i = element->children; i != NULL; i = i->next)
    {
      if(i->type == XML_ELEMENT_NODE)
      {   
        pair<string, int> p;
        p = evalString(i);
        value.append(p.first);
        position = p.second;
      }
    }
    return make_pair(value,position);
  }
  else if(!xmlStrcmp(element->name, (const xmlChar *) "lu"))
  {
    string myword;
    // best_blank_pos blankfrom;
    for(xmlNode *i = element->children; i != NULL; i = i->next)
    {
       if(i->type == XML_ELEMENT_NODE)
       {  
         pair<string, int> p;
         p = evalString(i);
         myword.append(p.first);
         position = p.second;
         // blankfrom = wordBlankPos(i, blankfrom);
       }
    }
    
    if(myword != "")
    {
      // int num = blankfrom.first;
      // wcerr << "blankfrom.first is " << num << "and position is " << position << endl;
      string s="";
      if(position >= 0 && position < number )
          s = *wordbound[position];
      return make_pair(s+"^"+myword+"$", position);
    }
    else
    {
      return make_pair("", position);
    }
  }
  else if(!xmlStrcmp(element->name, (const xmlChar *) "mlu"))
  {
    string value;
     	  
    bool first_time = true;
    
    for(xmlNode *i = element->children; i != NULL; i = i->next)
    {
      if(i->type == XML_ELEMENT_NODE)
      {
        string myword;
	 
        for(xmlNode *j = i->children; j != NULL; j = j->next)
        {
          if(j->type == XML_ELEMENT_NODE)
	  {       
            pair<string, int> p;
            p = evalString(j);
            myword.append(p.first);
            position = p.second;
	  }
        }
	      
	if(!first_time)
	{
	  if(myword != "" && myword[0] != '#')  //'+#' problem
	  {
	    value.append("+");
          }
	}
	else
	{
	  if(myword != "")
	  {
	    first_time = false;
          }
	}
	 
	value.append(myword);
      }
    }

    if(value != "")
    {
      string s="";
      make_pair("^"+value+"$", position);
    }
    else
    {
      return make_pair("", position);
    }
  }
  else if(!xmlStrcmp(element->name, (const xmlChar *) "chunk"))
  {
    return make_pair(processChunk(element), -2);
  }
  else
  {
    cerr << "Error: unexpected rvalue expression '" << element->name << "'" << endl;
    exit(EXIT_FAILURE);
  }

  return evalString(element);
}

void
Transfer::processOut(xmlNode *localroot)
{
  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(defaultAttrs == lu)
      {
        if(!xmlStrcmp(i->name, (const xmlChar *) "lu"))
        {
  	  string myword;
      // best_blank_pos blankfrom;
	  for(xmlNode *j = i->children; j != NULL; j = j->next)
	  {
	    if(j->type == XML_ELEMENT_NODE)
	    {  
        pair<string, int> p;
        p = evalString(j);
	      myword.append(p.first);
        position = p.second;
        // blankfrom = wordBlankPos(j, blankfrom);
            }
	  }
	  if(myword != "")
	  {  
      string e="";
      // int num = blankfrom.first;
      // wcerr << "blankfrom.first is " << num << "and position is " << position << endl;
      if(position >= 0 && position < number )
      {   
          output_string += UtfConverter::fromUtf8(*wordbound[position]);
          // fputws_unlocked(UtfConverter::fromUtf8(*wordbound[position]).c_str(), output);
  	   }
        output_string += L'^';
        output_string += UtfConverter::fromUtf8(myword);
        output_string += L'$';
     //    fputwc_unlocked(L'^', output);
   	 //    fputws_unlocked(UtfConverter::fromUtf8(myword).c_str(), output);
	    // fputwc_unlocked(L'$', output);
          }
        }
        else if(!xmlStrcmp(i->name, (const xmlChar *) "mlu"))
        {
          // wcout << "\npresent value is " << present << endl;
          string value;
	  // fputwc_unlocked('^', output);
	  bool first_time = true;
	  for(xmlNode *j = i->children; j != NULL; j = j->next)
	  {
	    if(j->type == XML_ELEMENT_NODE)
	    {
              string myword;
	      for(xmlNode *k = j->children; k != NULL; k = k->next)
	      {
	        if(k->type == XML_ELEMENT_NODE)
	        {       pair<string, int> p;
                  p = evalString(k);
                  myword.append(p.first);
	        }
	      }
	      
	      if(!first_time)
	      {
	        if(myword != "" && myword[0] != '#')  //'+#' problem
	        {
	          value.append("+");
                }
	      }
	      else
	      {
	        if(myword != "")
	        {
	          first_time = false;
                }
	      }
	      value.append(myword);
	    }
	  }
    // string e="";
    output_string += L'^'+UtfConverter::fromUtf8(value)+ L'$';
   //  fputwc_unlocked(L'^',output);
   //  fputws_unlocked(UtfConverter::fromUtf8(value).c_str(), output);
	  // fputwc_unlocked(L'$', output);
        }
        else // 'b'
        { 
          pair<string, int> p;
          p = evalString(i);
          position = p.second;
          output_string += UtfConverter::fromUtf8(p.first);
          // fputws_unlocked(UtfConverter::fromUtf8(p.first).c_str(), 
			  // output);
        }
      }
      else 
      {
        if(!xmlStrcmp(i->name, (const xmlChar *) "chunk"))
        { 
          output_string += UtfConverter::fromUtf8(processChunk(i));
          // fputws_unlocked(UtfConverter::fromUtf8(processChunk(i)).c_str(), output);
        }
        else // 'b'
        { 
          pair<string, int> p;
          p = evalString(i);
          position = p.second;
          output_string += UtfConverter::fromUtf8(p.first); 
          // fputws_unlocked(UtfConverter::fromUtf8(p.first).c_str(), output);
        }        
      }
    }
  }
}

string
Transfer::processChunk(xmlNode *localroot)
{
  string name, namefrom;
  string caseofchunk = "aa";
  string result;
  in_chunk = true;
      
  
  for(xmlAttr *i = localroot->properties; i != NULL; i = i->next)
  {
    if(!xmlStrcmp(i->name, (const xmlChar *) "name"))
    {
      name = (const char *) i->children->content;
    }
    else if(!xmlStrcmp(i->name, (const xmlChar *) "namefrom"))
    {
      namefrom = (const char *) i->children->content;
    }
    else if(!xmlStrcmp(i->name, (const xmlChar *) "case"))
    {
      caseofchunk = (const char *) i->children->content;
    }
  }

  result.append("^");
  if(caseofchunk != "")
  {
    if(name != "")
    {
      result.append(copycase(variables[caseofchunk], name));
    }
    else if(namefrom != "")
    {
      result.append(copycase(variables[caseofchunk], variables[namefrom]));
    }
    else
    {
      cerr << "Error: you must specify either 'name' or 'namefrom' for the 'chunk' element" << endl;
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    if(name != "")
    {
      result.append(name);
    }
    else if(namefrom != "")
    {
      result.append(variables[namefrom]);
    }
    else
    {
      cerr << "Error: you must specify either 'name' or 'namefrom' for the 'chunk' element" << endl;
      exit(EXIT_FAILURE);
    }
  }
  
  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(!xmlStrcmp(i->name, (const xmlChar *) "tags"))
      {
        result.append(processTags(i));
        result.append("{");
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "lu"))
      {
        string myword;
        // best_blank_pos blankfrom;
        for(xmlNode *j = i->children; j != NULL; j = j->next)
        {
          if(j->type == XML_ELEMENT_NODE)
          { 
            pair<string, int> p;
            p = evalString(j);
            myword.append(p.first);
            position = p.second;
            // blankfrom = wordBlankPos(j, blankfrom);
          }
        }
        string e="";
        if(myword != "")
        { 
          // int num = blankfrom.first;
          // wcerr << "blankfrom.first is " << num << "and position is " << position << endl;
          if(position >= 0 && position < number )
          {
              result.append(*wordbound[position]);
          }
          result.append("^");
          result.append(myword);
          result.append("$");
        }
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "mlu"))
      {
        bool first_time = true;
        string myword;
        for(xmlNode *j = i->children; j != NULL; j = j->next)
        {
          string mylocalword;
          if(j->type == XML_ELEMENT_NODE)
          {
            for(xmlNode *k = j->children; k != NULL; k = k->next)
            {
              if(k->type == XML_ELEMENT_NODE)
              {
                pair<string, int> p;
                p = evalString(k);
                position = p.second;
                mylocalword.append(p.first);
              }
            }
          
            if(!first_time)
            {
              if(mylocalword != "" && mylocalword[0] != '#')  // '+#' problem
              {
                myword += '+';	
              }
            }
            else
            {
              first_time = false;
            }
          }
          myword.append(mylocalword);
        }
        if(myword != "")
        { 
          result.append("^");
          result.append(myword);
          result.append("$");
        }
      }
      else // 'b'
      { 
        pair<string, int> p;
        p = evalString(i);
        position = p.second;
        result.append(p.first);
      }
    }
  }
  result.append("}$");
  in_chunk = false;
  return result;
}

string
Transfer::processTags(xmlNode *localroot)
{
  string result;
  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(!xmlStrcmp(i->name, (xmlChar const *) "tag"))
      {
        for(xmlNode *j = i->children; j != NULL; j = j->next)
        {
          if(j->type == XML_ELEMENT_NODE)
          { 
            pair<string, int> p;
            p = evalString(j);
            result.append(p.first);
            position = p.second;
          }
        }
      }
    }
  }
  return result;
}

void
Transfer::processInstruction(xmlNode *localroot)
{
  if(!xmlStrcmp(localroot->name, (const xmlChar *) "choose"))
  {
    processChoose(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "let"))
  {
    processLet(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "append"))
  {
    processAppend(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "out"))
  {
    processOut(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "call-macro"))
  {
    processCallMacro(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "modify-case"))
  {
    processModifyCase(localroot);
  }
}

void
Transfer::processLet(xmlNode *localroot)
{
  xmlNode *leftSide = NULL, *rightSide = NULL;

  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(leftSide == NULL)
      {
	leftSide = i;
      }
      else
      {
	rightSide = i;
	break;
      }
    }
  }

  map<xmlNode *, TransferInstr>::iterator it = evalStringCache.find(leftSide);
  if(it != evalStringCache.end())
  {
    TransferInstr &ti = it->second;
    switch(ti.getType())
    {
      case ti_var:
        variables[ti.getContent()] = evalString(rightSide).first;
        return;
        
      case ti_clip_sl:
        word[ti.getPos()]->setSource(attr_items[ti.getContent()], evalString(rightSide).first, ti.getCondition());
        return;
      
      case ti_clip_tl:
        word[ti.getPos()]->setTarget(attr_items[ti.getContent()], evalString(rightSide).first, ti.getCondition());
        return;      
        
      default:
        return;
    }
  }
  if(leftSide->name != NULL && !xmlStrcmp(leftSide->name, (const xmlChar *) "var"))
  {
    string const val = (const char *) leftSide->properties->children->content;
    variables[val] = evalString(rightSide).first;
    evalStringCache[leftSide] = TransferInstr(ti_var, val, 0);
  }
  else if(!xmlStrcmp(leftSide->name, (const xmlChar *) "clip"))
  {
    int pos = 0;
    xmlChar *part = NULL, *side = NULL, *as = NULL;
    bool queue = true;

    for(xmlAttr *i = leftSide->properties; i != NULL; i = i->next)
    {
      if(!xmlStrcmp(i->name, (const xmlChar *) "side"))
      {
	side = i->children->content;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "part"))
      {
	part = i->children->content;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "pos"))
      {
	pos = atoi((const char *) i->children->content) - 1;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "queue"))
      {
        if(!xmlStrcmp(i->children->content, (const xmlChar *) "no"))
        {
          queue = false;
        }
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "link-to"))
      {
        as = i->children->content;
      }      
    }
    
    if(!xmlStrcmp(side, (const xmlChar *) "tl"))
    {
      word[pos]->setTarget(attr_items[(const char *) part], evalString(rightSide).first, queue);
      evalStringCache[leftSide] = TransferInstr(ti_clip_tl, (const char *) part, pos, NULL, queue);
    }
    else
    {
      word[pos]->setSource(attr_items[(const char *) part], evalString(rightSide).first, queue);
      evalStringCache[leftSide] = TransferInstr(ti_clip_sl, (const char *) part, pos, NULL, queue);
    }    
  }
}

void
Transfer::processAppend(xmlNode *localroot)
{
  string name;
  for(xmlAttr *i = localroot->properties; i != NULL; i = i->next)
  {
    if(!xmlStrcmp(i->name, (const xmlChar *) "n"))
    {
      name = (char *) i->children->content; 
      break;
    }
  }

  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      variables[name].append(evalString(i).first);
    }
  }
}

void
Transfer::processModifyCase(xmlNode *localroot)
{
  xmlNode *leftSide = NULL, *rightSide = NULL;

  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(leftSide == NULL)
      {
	leftSide = i;
      }
      else
      {
	rightSide = i;
	break;
      }
    }
  }

  if(leftSide->name != NULL && !xmlStrcmp(leftSide->name, (const xmlChar *) "clip"))
  {
    int pos = 0;
    xmlChar *part = NULL, *side = NULL, *as = NULL;
    bool queue = true;

    for(xmlAttr *i = leftSide->properties; i != NULL; i = i->next)
    {
      if(!xmlStrcmp(i->name, (const xmlChar *) "side"))
      {
	side = i->children->content;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "part"))
      {
	part = i->children->content;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "pos"))
      {
	pos = atoi((const char *) i->children->content) - 1;
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "queue"))
      {
        if(!xmlStrcmp(i->children->content, (xmlChar const *) "no"))
        {
          queue = false;
        }
      }  
      else if(!xmlStrcmp(i->name, (const xmlChar *) "link-to"))
      {
        as = i->children->content;
      }
    }
    if(!xmlStrcmp(side, (const xmlChar *) "sl"))
    {
      string const result = copycase(evalString(rightSide).first, 
				      word[pos]->source(attr_items[(const char *) part], queue));
      word[pos]->setSource(attr_items[(const char *) part], result);
    }
    else
    {
      string const result = copycase(evalString(rightSide).first, 
				     word[pos]->target(attr_items[(const char *) part], queue));
      word[pos]->setTarget(attr_items[(const char *) part], result);
    }
  }
  else if(!xmlStrcmp(leftSide->name, (const xmlChar *) "var"))
  {
    string const val = (const char *) leftSide->properties->children->content;
    variables[val] = copycase(evalString(rightSide).first, variables[val]);
  }
}

void
Transfer::processCallMacro(xmlNode *localroot)
{
  string const n = (const char *) localroot->properties->children->content;
  int npar = 0;

  xmlNode *macro = macro_map[macros[n]];

  for(xmlAttr *i = macro->properties; i != NULL; i = i->next)
  {
    if(!xmlStrcmp(i->name, (const xmlChar *) "npar"))
    {
      npar = atoi((const char *) i->children->content);
      break;
    }
  }

  TransferWord **myword = NULL;
  if(npar > 0)
  {
    myword = new TransferWord *[npar];  
  }
  string **myblank = NULL;
  if(npar > 0)
  {
    myblank = new string *[npar];
    myblank[npar-1] = &emptyblank;
  }

  int idx = 0;
  int lastpos = 0;
  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      int pos = atoi((const char *) i->properties->children->content)-1;
      myword[idx] = word[pos];
      if(idx-1 >= 0)
      {
        myblank[idx-1] = wordbound[lastpos];
      }
      idx++;
      lastpos = pos;
    }
  }

  swap(myword, word);
  swap(myblank, wordbound);
  swap(npar, lword);
  
  for(xmlNode *i = macro->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      processInstruction(i);
    }
  }

  swap(myword, word);
  swap(myblank, wordbound);
  swap(npar, lword);
  
  if(myword)
  {
    delete[] myword;
  }
  if(myblank)
  {
    delete[] myblank;
  }
}

void
Transfer::processChoose(xmlNode *localroot)
{
  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(!xmlStrcmp(i->name, (const xmlChar *) "when"))
      {
        bool picked_option = false;
        
	for(xmlNode *j = i->children; j != NULL; j = j->next)
	{
	  if(j->type == XML_ELEMENT_NODE)
	  {
	    if(!xmlStrcmp(j->name, (const xmlChar *) "test"))
	    {
	      if(!processTest(j))
	      {
		break;
	      }
	      else
	      {
	        picked_option = true;
              }
	    }
	    else
	    {
	      processInstruction(j);
	    }
	  }
	}
        if(picked_option)
        {
          return;
        }	
      }
      else if(!xmlStrcmp(i->name, (const xmlChar *) "otherwise"))
      {
	for(xmlNode *j = i->children; j != NULL; j = j->next)
	{
	  if(j->type == XML_ELEMENT_NODE)
	  {
	    processInstruction(j);
	  }
	}
      }
    }
  }
}

bool
Transfer::processLogical(xmlNode *localroot)
{
  if(!xmlStrcmp(localroot->name, (const xmlChar *) "equal"))
  {
    return processEqual(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "begins-with"))
  {
    return processBeginsWith(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "begins-with-list"))
  {
    return processBeginsWithList(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "ends-with"))
  {
    return processEndsWith(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "ends-with-list"))
  {
    return processEndsWithList(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "contains-substring"))
  {
    return processContainsSubstring(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "or"))
  {
    return processOr(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "and"))
  {
    return processAnd(localroot);
  }
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "not"))
  {
    return processNot(localroot);
  } 
  else if(!xmlStrcmp(localroot->name, (const xmlChar *) "in"))
  {
    return processIn(localroot);
  }

  return false;
}

bool
Transfer::processIn(xmlNode *localroot)
{
  xmlNode *value = NULL;
  xmlChar *idlist = NULL;

  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    { 
      if(value == NULL)
      {
	value = i;
      }
      else
      {
	idlist = i->properties->children->content;
	break;
      }
    }
  }

  string sval = evalString(value).first;

  if(localroot->properties != NULL)
  {
    if(!xmlStrcmp(localroot->properties->children->content, 
		  (const xmlChar *) "yes"))
    {
      set<string, Ltstr> &myset = listslow[(const char *) idlist];
      if(myset.find(tolower(sval)) != myset.end())
      {
	return true;
      }
      else
      {
	return false;
      }
    }
  }

  set<string, Ltstr> &myset = lists[(const char *) idlist];
  if(myset.find(sval) != myset.end())
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool
Transfer::processTest(xmlNode *localroot)
{
  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      return processLogical(i);
    }
  }  
  return false;
}

bool
Transfer::processAnd(xmlNode *localroot)
{
  bool val = true;
  for(xmlNode *i = localroot->children; val && i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      val = val && processLogical(i);
    }
  }

  return val;
}

bool
Transfer::processOr(xmlNode *localroot)
{
  bool val = false;
  for(xmlNode *i = localroot->children; !val && i != NULL ; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      val = val || processLogical(i);
    }
  }

  return val;
}

bool
Transfer::processNot(xmlNode *localroot)
{
  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      return !processLogical(i);
    }
  }
  return false;
}

bool
Transfer::processEqual(xmlNode *localroot)
{
  xmlNode *first = NULL, *second = NULL;

  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(first == NULL)
      {
        first = i;
      }
      else
      {
	second = i;
	break;
      }
    }
  }

  if(localroot->properties == NULL)
  {
    return evalString(first).first == evalString(second).first;
  }
  else
  {
    if(!xmlStrcmp(localroot->properties->children->content,
		  (const xmlChar *) "yes"))
    {
      return tolower(evalString(first).first) == tolower(evalString(second).first);
    }
    else
    { 
      return evalString(first).first == evalString(second).first;
    }
  }
}

bool
Transfer::beginsWith(string const &s1, string const &s2) const
{
  int const limit = s2.size(), constraint = s1.size();
  
  if(constraint < limit)
  {
    return false;
  }
  for(int i = 0; i != limit; i++)
  {
    if(s1[i] != s2[i])
    {
      return false;
    }
  }

  return true;
}

bool
Transfer::endsWith(string const &s1, string const &s2) const
{
  int const limit = s2.size(), constraint = s1.size();
  
  if(constraint < limit)
  {
    return false;
  }
  for(int i = limit-1, j = constraint - 1; i >= 0; i--, j--)
  {
    if(s1[j] != s2[i])
    {
      return false;
    }
  }

  return true;
}


bool
Transfer::processBeginsWith(xmlNode *localroot)
{
  xmlNode *first = NULL, *second = NULL;

  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(first == NULL)
      {
        first = i;
      }
      else
      {
	second = i;
	break;
      }
    }
  }

  if(localroot->properties == NULL)
  {
    return beginsWith(evalString(first).first, evalString(second).first);
  }
  else
  {
    if(!xmlStrcmp(localroot->properties->children->content,
		  (const xmlChar *) "yes"))
    {
      return beginsWith(tolower(evalString(first).first), tolower(evalString(second).first));
    }
    else
    {
      return beginsWith(evalString(first).first, evalString(second).first);
    }
  }
}

bool
Transfer::processEndsWith(xmlNode *localroot)
{
  xmlNode *first = NULL, *second = NULL;

  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(first == NULL)
      {
        first = i;
      }
      else
      {
	second = i;
	break;
      }
    }
  }

  if(localroot->properties == NULL)
  {
    return endsWith(evalString(first).first, evalString(second).first);
  }
  else
  {
    if(!xmlStrcmp(localroot->properties->children->content,
		  (const xmlChar *) "yes"))
    {
      return endsWith(tolower(evalString(first).first), tolower(evalString(second).first));
    }
    else
    {
      return endsWith(evalString(first).first, evalString(second).first);
    }
  }
}

bool
Transfer::processBeginsWithList(xmlNode *localroot)
{
  xmlNode *first = NULL, *second = NULL;

  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(first == NULL)
      {
        first = i;
      }
      else
      {
	second = i;
	break;
      }
    }
  }

  xmlChar *idlist = second->properties->children->content;
  string needle = evalString(first).first;
  set<string, Ltstr>::iterator it, limit;

  if(localroot->properties == NULL || 
     xmlStrcmp(localroot->properties->children->content, (const xmlChar *) "yes"))
  {
    it = lists[(const char *) idlist].begin();
    limit = lists[(const char *) idlist].end();
  }
  else
  {
    needle = tolower(needle);
    it = listslow[(const char *) idlist].begin();
    limit = listslow[(const char *) idlist].end();
  }
  
  for(; it != limit; it++)
  {
    if(beginsWith(needle, *it))
    {
      return true;
    }
  }
  return false;
}


bool
Transfer::processEndsWithList(xmlNode *localroot)
{
  xmlNode *first = NULL, *second = NULL;

  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(first == NULL)
      {
        first = i;
      }
      else
      {
	second = i;
	break;
      }
    }
  }

  xmlChar *idlist = second->properties->children->content;
  string needle = evalString(first).first;
  set<string, Ltstr>::iterator it, limit;

  if(localroot->properties == NULL || 
     xmlStrcmp(localroot->properties->children->content, (const xmlChar *) "yes"))
  {
    it = lists[(const char *) idlist].begin();
    limit = lists[(const char *) idlist].end();
  }
  else
  {
    needle = tolower(needle);
    it = listslow[(const char *) idlist].begin();
    limit = listslow[(const char *) idlist].end();
  }
  
  for(; it != limit; it++)
  {
    if(endsWith(needle, *it))
    {
      return true;
    }
  }
  return false;
}

bool
Transfer::processContainsSubstring(xmlNode *localroot)
{
  xmlNode *first = NULL, *second = NULL;

  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(first == NULL)
      {
        first = i;
      }
      else
      {
	second = i;
	break;
      }
    }
  }

  if(localroot->properties == NULL)
  {
    return evalString(first).first.find(evalString(second).first) != string::npos;
  }
  else
  {
    if(!xmlStrcmp(localroot->properties->children->content,
		  (const xmlChar *) "yes"))
    {
      return tolower(evalString(first).first).find(tolower(evalString(second).first)) != string::npos;
    }
    else
    {
      return evalString(first).first.find(evalString(second).first) != string::npos;
    }
  }
}

string
Transfer::copycase(string const &source_word, string const &target_word)
{
  wstring result;
  wstring const s_word = UtfConverter::fromUtf8(source_word);
  wstring const t_word = UtfConverter::fromUtf8(target_word);

  bool firstupper = iswupper(s_word[0]);
  bool uppercase = firstupper && iswupper(s_word[s_word.size()-1]);
  bool sizeone = s_word.size() == 1;

  if(!uppercase || (sizeone && uppercase))
  {
    result = t_word;
    result[0] = towlower(result[0]);
    //result = StringUtils::tolower(t_word);
  }
  else
  {
    result = StringUtils::toupper(t_word);
  }
  
  if(firstupper)
  {
    result[0] = towupper(result[0]);
  }
   
  return UtfConverter::toUtf8(result);
}

string 
Transfer::caseOf(string const &str)
{
  wstring const s = UtfConverter::fromUtf8(str);

  if(s.size() > 1)
  {
    if(!iswupper(s[0]))
    {
      return "aa";
    }
    else if(!iswupper(s[s.size()-1]))
    {
      return "Aa";
    }
    else
    {
      return "AA";
    }
  }
  else if(s.size() == 1)
  {
    if(!iswupper(s[0]))
    {
      return "aa";
    }
    else
    {
      return "Aa";
    }
  }
  else
  {
    return "aa";
  }
}

string
Transfer::tolower(string const &str) const
{
  return UtfConverter::toUtf8(StringUtils::tolower(UtfConverter::fromUtf8(str)));
}

string
Transfer::tags(string const &str) const
{
  string result = "<";

  for(unsigned int i = 0, limit = str.size(); i != limit; i++)
  {
    if(str[i] == '.')
    {
      result.append("><");
    }
    else
    {
      result += str[i];
    }
  }
  
  result += '>';

  return result;
}

void
Transfer::processRule(xmlNode *localroot)
{
  // localroot is suposed to be an 'action' tag
  for(xmlNode *i = localroot->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      processInstruction(i);
    }
  }
}

TransferToken &
Transfer::readToken(FILE *in)
{
  if(!input_buffer.isEmpty())
  {
    return input_buffer.next();
  }

  wstring content, preblank;
  int superend = 0;
  int formatstart = -1;
  while(true)
  {
    int val = fgetwc_unlocked(in);
    if(feof(in) || (val == 0 && internal_null_flush))
    {
      return input_buffer.add(TransferToken(tt_eof, content));
    }
    if(val == '\\')
    {
      content += L'\\';
      content += wchar_t(fgetwc_unlocked(in));
    }
    else if(val == L'[')
    {
      content += L'[';
      int val2 = fgetwc_unlocked(in);
      bool is_format;
      if(val2 == L'{')
      {
        formatstart = content.size()-1;
        is_format = true;
      }
      else
      {
        is_format = false;
      }
      while(true)
      {
        content += wchar_t(val2);
        if(val2 == L'\\')
        {
          content += wchar_t(fgetwc_unlocked(in));
        }
        else if(val2 == L']')
        {
          if(!is_format)
          {
            superend = content.size();
          }
          break;
        }
        val2 = fgetwc_unlocked(in);
      }
    }
    else if(val == L'$')
    {
      return input_buffer.add(TransferToken(content, tt_word,
                                            preblank, superend, formatstart));
    }
    else if(val == L'^')
    {
      preblank.swap(content);
    }
    else if(val == L'\0' && null_flush)
    {
      fflush(output);
    }
    else
    {
      content += wchar_t(val);
    }
  }
}
bool
Transfer::getNullFlush(void)
{
  return null_flush;
}

void
Transfer::setNullFlush(bool null_flush)
{
  this->null_flush = null_flush;
}

void
Transfer::setTrace(bool trace)
{
  this->trace = trace;
}

void
Transfer::transfer_wrapper_null_flush(FILE *in, FILE *out)
{
  null_flush = false;
  internal_null_flush = true;
  
  while(!feof(in))
  {
    transfer(in, out);
    output_string += L'\0';
    // fputwc_unlocked(L'\0', out);
    int code = fflush(out);
    if(code != 0)
    {
      wcerr << L"Could not flush output " << errno << endl;
    }
  }

  internal_null_flush = false;
  null_flush = true;
}

wstring 
Transfer::emptyblanks(wstring const &str)
{
  wstring final = L"";
  int l = str.length();
  // wcerr << str << endl << endl;
  int j = 0;
  while(j < l-1)
  {
    if(str[j]==L'[' && str[j+1] == L']')
      j += 2;
    else
      final += str[j++];
  }
  if(j < l)
    final += str[j];

  return final;
}

void
Transfer::output_transfer(wstring const &str)
{
  int l = str.length();
  int j = 0;
  bool in_inline = false;
  // wcerr << str << endl << endl;
  wstring final = L"";
  while(j < l-1)
  {
    if(str[j] == L'$')
    { 
      final += L'$';
      j++;
      bool found = false;
      wstring temp = L"";
      while(str[j]!= L'^' && j < l-1)
      {
        if(str[j] == '[')
          found = true;
        temp += str[j++];
      }
      if(!found && in_inline)
      { 
        in_inline = false;
        temp = L"[]" + temp + str[j++];
      }
      final += temp;
    }
    else if(str[j] == L'[' && str[j+1] == L'{')
    {
      in_inline = true;
      final += L"[{";
      j += 2;
    }
    else
      final += str[j++];
  }
  if(j < l)
    final += str[j];

  fputws_unlocked(final.c_str(), output);
}

wstring
Transfer::firstTranslationOfWord(wstring const &word) const
{
  wstring tl;
  int seenSlash = 0;
  for(wstring::const_iterator it = word.begin(); it != word.end(); it++)
  {
    if(seenSlash == 0)
    {
      if(*it == L'/')
      {
        seenSlash++;
      }
      else if(*it == L'\\')
      {
        it++;
      }
    }
    else if(seenSlash == 1)
    {
      if(*it == L'/')
      {
        // reached end of first translation
        break;
      }
      else if(*it == L'\\')
      {
        tl.push_back(*it);
        it++;
        tl.push_back(*it);
      }
      else
      {
        tl.push_back(*it);
      }
    }
  }
  return tl;
}

void
Transfer::applyDefaultRule(TransferToken &token)
{
  output_string += emptyblanks(token.getSuperblank());
  output_string += emptyblanks(token.getFreeblank());
  // fputws_unlocked(token.getSuperblank().c_str(), output);
  // fputws_unlocked(token.getFreeblank().c_str(), output);
  wstring const &word = token.getContent();
  pair<wstring, int> tr;
  if(useBilingual && preBilingual == false)
  {
    if(isExtended && (word)[0] == L'*')
    {
      tr = extended.biltransWithQueue((word).substr(1), false);
      if(tr.first[0] == L'@')
      {
        tr.first[0] = L'*';
      }
      else
      {
        tr.first = L"%" + tr.first;
      }
    }
    else
    {
      tr = fstp.biltransWithQueue(word, false);
    }
  }
  else if(preBilingual)
  {
    wstring tl = firstTranslationOfWord(word);
    tr = pair<wstring, int>(tl, false);
  }
  else
  {
    tr = pair<wstring, int>(word, 0);
  }

  if(tr.first.size() != 0)
  {
    if(defaultAttrs == lu)
    {
      output_string += token.getFormat();
      output_string += L'^' + tr.first + L'$';
      // fputws_unlocked(token.getFormat().c_str(), output);
      // fputwc_unlocked(L'^', output);
      // fputws_unlocked(tr.first.c_str(), output);
      // fputwc_unlocked(L'$', output);
    }
    else
    {
      if(tr.first[0] == '*')
      { 
        output_string += L"^unknown<unknown>{";
        // fputws_unlocked(L"^unknown<unknown>{", output);
      }
      else
      {
        output_string += L"^default<default>{";
        // fputws_unlocked(L"^default<default>{", output);
      }
      output_string += token.getFormat() + L"^" + tr.first + L"$}$";
      // fputws_unlocked(token.getFormat().c_str(), output);
      // fputws_unlocked(L"^", output);
      // fputws_unlocked(tr.first.c_str(), output);
      // fputws_unlocked(L"$}$", output);
    }
  }
}

void
Transfer::transfer(FILE *in, FILE *out)
{
  if(getNullFlush())
  {
    transfer_wrapper_null_flush(in, out);
  }
  
  int last_out = 0;

  output = out;
  position = -2;
  ms.init(me->getInitial());
  output_string = L"";
  
  while(true)
  {
    if(ms.size() == 0)
    {
      if(lastrule != NULL)
      {
        tmpword.pop_back();
        for(unsigned int ind = 0; ind < tmpword.size(); ind++)
        {
          output_string += emptyblanks(tmpword[ind]->getSuperblank());
          // fputws_unlocked(tmpword[ind]->getSuperblank().c_str(), output);
        }
      	applyRule(lastrule);
        lastrule = NULL;
        tmpword.clear();
        ms.init(me->getInitial());
        input_buffer.setPos(last_out);
      }
    	else if(tmpword.size() != 0)
    	{  
        applyDefaultRule(*tmpword[0]);
        tmpword.clear();
        input_buffer.setPos(last_out);
        input_buffer.next();
        last_out = input_buffer.getPos();
        ms.init(me->getInitial());
      }
    }
    int val = ms.classifyFinals(me->getFinals());
    if(val != -1)
    {
      lastrule = rule_map[val-1];      
      last_out = input_buffer.getPos();
      wcerr << L"finals found, last_out set to " << last_out <<endl;

      if(trace)
      {
        wcerr << endl << L"apertium-transfer: Rule " << val << L" ";
        for (unsigned int ind = 0; ind < tmpword.size(); ind++)
        {
          if (ind != 0)
          {
            wcerr << L" ";
          }
          wcerr << tmpword[ind]->getContent();
        }
        wcerr << endl;
      }
    }

    TransferToken &current = readToken(in);
    // wcout << "Token is " << current.getContent() << endl;

   
    switch(current.getType())
    {
      case tt_word:
	      wcerr <<L"tt_word, tmpword.size()=="<<tmpword.size()<<L" word="<<current.getContent()<<endl;
        if(tmpword.size() > 0) {
          ms.step(L' ');
        }
        tmpword.push_back(&current);
        applyWord(current.getContent());
	    break;


      case tt_eof:
        wcerr <<L"tt_eof, tmpword.size()=="<<tmpword.size()<<L" word="<<current.getContent()<<endl;
        if(tmpword.size() > 0) {
          // We still have words to print, but clear match state since
          // it's not possible to match longer rule sequences:
          ms.clear();
          tmpword.push_back(&current);
          break;
        }
        else {
          output_string += emptyblanks(current.getSuperblank());
          output_transfer(output_string);
          // fputws_unlocked(output_string.c_str(), output);
          return;
        }
	    break;

      default:
	cerr << "Error: Unknown input token." << endl;
	return;
    }
  }
  // wcerr << "Output string is " << output_string << "this" << endl;
  // fputws_unlocked(output_string.c_str(),output);
}

void
Transfer::applyRule(xmlNode *rule)
{
  unsigned int limit = tmpword.size();
  number = limit;

  wcerr << L"applyRule: " << tmpword.size() << endl;
  
  
  for(unsigned int i = 0; i != limit; i++)
  {
    if(i == 0)
    {
      word = new TransferWord *[limit];
      wordbound = new string *[limit];
      // wordbound[0] = new string(UtfConverter::toUtf8(last_wordblank));
      output_string += emptyblanks(tmpword[i]->getFreeblank());
      // fputws_unlocked(tmpword[i]->getFreeblank().c_str(), output);
      
      lword = limit;

    }
    else if(!tmpword[i]->getFreeblank().empty())
    {
      freeblank.push_back(tmpword[i]->getFreeblank());
    }
    wordbound[i] = new string(UtfConverter::toUtf8(tmpword[i]->getFormat()));
    
    pair<wstring, int> tr;
    if(useBilingual && preBilingual == false)
    {
      tr = fstp.biltransWithQueue(tmpword[i]->getContent(), false);
    }
    else if(preBilingual)
    {
      wstring tl = firstTranslationOfWord(tmpword[i]->getContent());
      //tmpword[i]->assign(sl); 
      tr = pair<wstring, int>(tl, false);
    }
    else
    {
      tr = pair<wstring, int>(tmpword[i]->getContent(), false);
    }

    word[i] = new TransferWord(UtfConverter::toUtf8(tmpword[i]->getContent()), 
			       UtfConverter::toUtf8(tr.first), tr.second);
    wcerr <<L"word["<<i<<"]="<<tmpword[i]->getContent();
    wcerr <<L"\tformat["<<i<<"]="<<tmpword[i]->getFormat();
    wcerr <<L"\tfreeblank["<<i<<"]="<<tmpword[i]->getFreeblank()<<endl;
  }


  // fputws_unlocked(superblanks.c_str(),output);
  wcerr <<"processRule"<<endl;
  processRule(lastrule);
  lastrule = NULL;

  if(word)
  {
    for(unsigned int i = 0; i != limit; i++)
    {
      delete word[i];
    }
    delete[] word;
  }
  if(!freeblank.empty())
  {
    for(std::deque<wstring>::const_iterator it = freeblank.begin(); it != freeblank.end(); it++)
    {
      output_string += it->c_str();
      // fputws_unlocked(it->c_str(), output);
    }
  }

  // if(blank)
  // {
  //   for(unsigned int i = 0; i != limit - 1; i++)
  //   {
  //     delete blank[i];
  //   }
  //   delete[] blank;
  // }

  if(wordbound)
  {
    for(unsigned int i = 0; i != limit - 1; i++)
    {
      delete wordbound[i];
    }
    delete[] wordbound;
  }
  word = NULL;
  blank = NULL;
  wordbound = NULL;
  // tmpword.clear();
  // tmpblank.clear();
  // ms.init(me->getInitial());
}

/* HERE */
void
Transfer::applyWord(wstring const &word_str)
{
  ms.step(L'^');
  for(unsigned int i = 0, limit = word_str.size(); i < limit; i++)
  {
    switch(word_str[i])
    {
      case L'\\':
        i++;
	ms.step(towlower(word_str[i]), any_char);
	break;

      case L'/':
        i = limit;
        break;

      case L'<':
	for(unsigned int j = i+1; j != limit; j++)
	{
	  if(word_str[j] == L'>')
	  {
	    int symbol = alphabet(word_str.substr(i, j-i+1));
	    if(symbol)
	    {
	      ms.step(symbol, any_tag);
	    }
	    else
	    {
	      ms.step(any_tag);
	    }
	    i = j;
	    break;
	  }
	}
	break;
	
      default:
	ms.step(towlower(word_str[i]), any_char);
	break;
    }
  }
  ms.step(L'$');
}

void 
Transfer::setPreBilingual(bool value) 
{
  preBilingual = value;
}

bool
Transfer::getPreBilingual(void) const
{
  return preBilingual;
}

void 
Transfer::setUseBilingual(bool value) 
{
  useBilingual = value;
}

bool
Transfer::getUseBilingual(void) const
{
  return useBilingual;
}

void
Transfer::setCaseSensitiveness(bool value)
{
  fstp.setCaseSensitiveMode(value);
}
