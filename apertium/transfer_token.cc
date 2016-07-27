/*
 * Copyright (C) 2005 Universitat d'Alacant / Universidad de Alicante
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
#include <apertium/transfer_token.h>
#include <apertium/string_utils.h>
#include <iostream>

using namespace Apertium;

void
TransferToken::copy(TransferToken const &o)
{
  type = o.type;
  content = o.content;
  superblank = o.superblank;
  freeblank = o.freeblank;
  format = o.format;
}

void
TransferToken::destroy()
{
}

TransferToken::TransferToken()
{
}

TransferToken::TransferToken(wstring const &content,
			     TransferTokenType type)
{
  this->content = content;
  this->type = type;
}


TransferToken::TransferToken(TransferTokenType type,
                             wstring const &superblank)
{
  this->type = type;
  this->superblank = superblank;
}

TransferToken::TransferToken(wstring const &content, TransferTokenType type,
                             wstring const &blank, int superend, int formatstart)
{
  this->content = content;
  this->type = type;

  int end = (int) blank.size();
  if(formatstart < 0) {
    formatstart = end;
  }
  if(0 <= superend && superend <= formatstart && formatstart <= end) {
    this->superblank = blank.substr(0, superend);
    this->freeblank = blank.substr(superend, formatstart-superend);
    this->format = blank.substr(formatstart);
  }
  else {
    // TODO: should we fail here? means caller has a bug
    wcerr << L"Warning in TransferToken::(): bad blank indices, making all super"<<endl;
    this->superblank = blank;
  }
}

TransferToken::TransferToken(wstring const &content, TransferTokenType type,
                             wstring const &blank)
{
  this->content = content;
  this->type = type;
  this->superblank = blank;
}

TransferToken::~TransferToken()
{
  destroy();
}

TransferToken::TransferToken(TransferToken const &o)
{
  copy(o);
}

TransferToken &
TransferToken::operator =(TransferToken const &o)
{
  if(this != &o)
  {
    destroy();
    copy(o);
  }
  return *this;
}

TransferTokenType
TransferToken::getType()
{
  return type;
}

// wstring &
// TransferToken::getWord()
// {
//   return word;
// }

wstring &
TransferToken::getSuperblank()
{
  return superblank;
}

wstring &
TransferToken::getFreeblank()
{
  return freeblank;
}

wstring &
TransferToken::getFormat()
{
  return format;
}


wstring & 
TransferToken::getContent()
{
  return content;
}

void 
TransferToken::setType(TransferTokenType type)
{
  this->type = type;
}

void 
TransferToken::setContent(wstring const &content)
{
  this->content = content;
}

// void
// TransferToken::setWord(wstring const &word)
// {
//   this->word = word;
// }

void
TransferToken::setSuperblank(wstring const &superblank)
{
  this->superblank = superblank;
}

void
TransferToken::setFreeblank(wstring const &freeblank)
{
  this->freeblank = freeblank;
}

void
TransferToken::setFormat(wstring const &format)
{
  this->format = format;
}


