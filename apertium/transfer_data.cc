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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <apertium/transfer_data.h>
#include <lttoolbox/compression.h>
#include <apertium/utf_converter.h>
#include <apertium/apertium_re.h>
#include <iostream>
#include <apertium/string_utils.h>

using namespace Apertium;
using namespace std;

void
TransferData::copy(TransferData const &o)
{
  alphabet = o.alphabet;
  transducer = o.transducer;
  finals = o.finals;
  attr_items = o.attr_items;
  macros = o.macros;
  lists = o.lists;
  variables = o.variables;
}

void
TransferData::destroy()
{
}

TransferData::TransferData()
{
  // adding fixed attr_items
  attr_items[L"lem"] = L"^(([^<]|\"\\<\")+)";
  attr_items[L"lemq"] = L"\\#[- _][^<]+";
  attr_items[L"lemh"] = L"^(([^<#]|\"\\<\"|\"\\#\")+)";
  attr_items[L"whole"] = L"(.+)";
  attr_items[L"tags"] = L"((<[^>]+>)+)";
  attr_items[L"chname"] = L"({([^/]+)\\/)"; // includes delimiters { and / !!!
  attr_items[L"chcontent"] = L"(\\{.+)";
  attr_items[L"content"] = L"(\\{.+)";
}

TransferData::~TransferData()
{
  destroy();
}

TransferData::TransferData(TransferData const &o)
{
  copy(o);
}

TransferData &
TransferData::operator =(TransferData const &o)
{
  if(this != &o)
  {
    destroy();
    copy(o);
  }
  return *this;
}

Alphabet &
TransferData::getAlphabet()
{
  return alphabet;
}

Transducer &
TransferData::getTransducer()
{
  return transducer;
}

map<int, int> &
TransferData::getFinals()
{
  return finals;
}

map<wstring, wstring, Ltstr> &
TransferData::getAttrItems()
{
  return attr_items;
}

map<wstring, int, Ltstr> &
TransferData::getMacros()
{
  return macros;
}

map<wstring, set<wstring, Ltstr>, Ltstr> &
TransferData::getLists()
{
  return lists;
}

map<wstring, wstring, Ltstr> &
TransferData::getVariables()
{
  return variables;
}

void
TransferData::write(FILE *output)
{
  alphabet.write(output);

  transducer.minimize();
  // Make a copy of the old finals:
  set<int> tr_finals = transducer.getFinals();
  // Final the new finals, and collect a map of them into their rule numbers:
  map<int, int> finals;
  map<int, multimap<int, int> >& transitions = transducer.getTransitions();
  for(map<int, multimap<int, int> >::const_iterator it = transitions.begin(),
        limit = transitions.end(); it != limit; ++it)
  {
    const int& src = it->first;
    for(multimap<int, int>::const_iterator arc = it->second.begin(),
          arclimit = it->second.end(); arc != arclimit; ++arc)
    {
      int label = arc->first;
      int trg = arc->second;
      if(alphabet.isTag(label)) {
        continue;
      }
      if(alphabet.decode(label).first != 0) {
        continue;
      }
      int rlabel = alphabet.decode(label).second;
      if(rlabel == 0) {
        continue;
      }
      if(!transducer.isFinal(trg)) {
        std::cerr << "Warning: src=" << src << " label=" << label << " decode(label)=(" << alphabet.decode(label).first << "," << alphabet.decode(label).second << ") non-final trg=" << trg;
        continue;
      }
      if(final_labels.count(label) == 0) {
        std::cerr << "Warning: src=" << src << " label=" << label << " decode(label)=(" << alphabet.decode(label).first << "," << alphabet.decode(label).second << ") trg=" <<trg << " not in final_labels";
        continue;
      }
      transducer.setFinal(src);
      finals[src] = rlabel;

      // std::cerr << "\033[1;35msrc= " << src << "\tlabel= " << label << " ;decode(label)=(" << alphabet.decode(label).first << "," << alphabet.decode(label).second << ")\033[0m\t";
      // std::cerr << "\033[1;35mtrg=\t" << trg << "\033[0m\t";
      // std::cerr << "\033[1;35misFinal(src)=" << transducer.isFinal(src) << " (trg)=" << transducer.isFinal(trg) << "\033[0m" << std::endl;
    }
  }
  for(set<int>::const_iterator it = tr_finals.begin(), limit = tr_finals.end(); it != limit; ++it)
  {
    transducer.setFinal(*it, false);
    // std::cerr << "\033[1;35munfinalling *it=" << (*it) << "\tisFinal=" << transducer.isFinal(*it) << std::endl;
  }

  transducer.write(output, alphabet.size());

  // finals

  Compression::multibyte_write(finals.size(), output);
  for(map<int, int>::const_iterator it = finals.begin(), limit = finals.end();
      it != limit; it++)
  {
    // std::cerr << "\033[1;35mwriting finals[" << it->first << "]=" << it->second << "\033[0m" << std::endl;
    Compression::multibyte_write(it->first, output);
    Compression::multibyte_write(it->second, output);
  }

  // attr_items

  // precompiled regexps
  writeRegexps(output);

  // variables
  Compression::multibyte_write(variables.size(), output);
  for(map<wstring, wstring, Ltstr>::const_iterator it = variables.begin(), limit = variables.end();
      it != limit; it++)
  {
    Compression::wstring_write(it->first, output);
    Compression::wstring_write(it->second, output);
  }

  // macros
  Compression::multibyte_write(macros.size(), output);
  for(map<wstring, int, Ltstr>::const_iterator it = macros.begin(), limit = macros.end();
      it != limit; it++)
  {
    Compression::wstring_write(it->first, output);
    Compression::multibyte_write(it->second, output);
  }

  // lists
  Compression::multibyte_write(lists.size(), output);
  for(map<wstring, set<wstring, Ltstr>, Ltstr>::const_iterator it = lists.begin(), limit = lists.end();
      it != limit; it++)
  {
    Compression::wstring_write(it->first, output);
    Compression::multibyte_write(it->second.size(), output);

    for(set<wstring, Ltstr>::const_iterator it2 = it->second.begin(), limit2 = it->second.end();
	it2 != limit2; it2++)
    {
      Compression::wstring_write(*it2, output);
    }
  }

}

void
TransferData::writeRegexps(FILE *output)
{
  Compression::string_write(string(pcre_version()), output);
  Compression::multibyte_write(attr_items.size(), output);

  map<wstring, wstring, Ltstr>::iterator it, limit;
  for(it = attr_items.begin(), limit = attr_items.end(); it != limit; it++)
  {
    Compression::wstring_write(it->first, output);
    ApertiumRE my_re;
    my_re.compile(UtfConverter::toUtf8(it->second));
    my_re.write(output);
    Compression::wstring_write(it->second, output);
  }
}
