//
//  ASN1DER.cpp
//  img4tool
//
//  Created by tihmstar on 04.10.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#include "ASN1DERElement.hpp"
#include <algorithm>
//#include <img4tool/libgeneral/macros.h>
#include <assert.h>
#include <string.h>
#include <string>

using namespace tihmstar::img4tool;

#pragma mark helper

std::string makeASN1Size(size_t size) {
  assert(size < 0x100000000);
  if (size >= 0x1000000) {
    // 1+4 bytes length
    return {{(char)0x84, (char)((size >> 24) & 0xFF),
             (char)((size >> 16) & 0xFF), (char)((size >> 8) & 0xFF),
             (char)(size & 0xFF)}};
  } else if (size >= 0x10000) {
    // 1+3 bytes length
    return {{(char)0x83, (char)((size >> 16) & 0xFF),
             (char)((size >> 8) & 0xFF), (char)(size & 0xFF)}};
  } else if (size >= 0x100) {
    // 1+2 bytes length
    return {{(char)0x82, (char)((size >> 8) & 0xFF), (char)(size & 0xFF)}};
  } else if (size >= 0x80) {
    // 1+1 byte length
    return {{(char)0x81, (char)(size & 0xFF)}};
  } else {
    // 1 byte length
    return {(char)(size & 0xFF)};
  }
}

#pragma mark ASN1DERElementIterator

ASN1DERElement::ASN1DERElementIterator::ASN1DERElementIterator(
    const ASN1DERElement::ASN1TAG *buf, size_t containerSize, uint64_t pos)
    : _buf(buf), _containerSize(containerSize), _pos(pos) {
  //
}

ASN1DERElement::ASN1DERElementIterator &ASN1DERElement::ASN1DERElementIterator::
operator++() {
  ASN1DERElement e(_buf + _pos, _containerSize - _pos);
  _pos = (uint64_t)e.buf() + e.size() - (uint64_t)_buf;
  assert(_pos <= _containerSize);
  return *this;
}

bool ASN1DERElement::ASN1DERElementIterator::
operator!=(const ASN1DERElementIterator &e) {
  return (_buf != e._buf || _pos != e._pos ||
          _containerSize != e._containerSize);
}

const ASN1DERElement ASN1DERElement::ASN1DERElementIterator::operator*() const {
  return {_buf + _pos, _containerSize - (size_t)_pos};
}

#pragma mark ASN1DERElement

ASN1DERElement::ASN1DERElement(const void *buf, size_t bufSize)
    : _buf((const ASN1TAG *)buf), _bufSize(bufSize), _ownsBuffer(false) {

  assert(_bufSize > 2); // needs at least TAG and Size
  if (((uint8_t *)_buf)[0] != 0xff) {
    assert(_buf->tagNumber <= TagBMPString);
  }
  assert(_bufSize >= size());
}

ASN1DERElement::ASN1DERElement(const ASN1TAG tag, const void *payload,
                               size_t payloadLen)
    : _buf(NULL), _bufSize(0), _ownsBuffer(true) {
  std::string size = makeASN1Size(payloadLen);
  assert(_buf =
             (const ASN1TAG *)malloc(_bufSize = 1 + payloadLen + size.size()));

  memcpy((void *)&_buf[0], &tag, 1);
  memcpy((void *)&_buf[1], size.c_str(), size.size());
  if (payloadLen) {
    memcpy((void *)&_buf[1 + size.size()], payload, payloadLen);
  }
}

ASN1DERElement::ASN1DERElement(ASN1DERElement &&old)
    : _buf(old._buf), _bufSize(old._bufSize), _ownsBuffer(old._ownsBuffer) {
  old._ownsBuffer = false;
}

ASN1DERElement::ASN1DERElement(const ASN1DERElement &old)
    : _buf(NULL), _bufSize(old._bufSize), _ownsBuffer(true) {
  _buf = (ASN1TAG *)malloc(_bufSize);
  memcpy((void *)&_buf[0], old._buf, _bufSize);
}

ASN1DERElement::~ASN1DERElement() {
  if (_ownsBuffer) {
    void *freeme = (void *)_buf;
    _buf = NULL;
    free(freeme);
  }
}

size_t ASN1DERElement::taginfoSize() const {
  size_t tagInfoSize = 0;
  if (((uint8_t *)_buf)[0] == 0xff) {
    ASN1PrivateTag *ptag = ((ASN1PrivateTag *)_buf) + 1;
    do {
      assert(_bufSize >= 2 + (++tagInfoSize));
    } while (ptag++->more);
  }

  ASN1Len *tlen = (ASN1Len *)&_buf[1 + tagInfoSize];
  return ((!tlen->isLong) ? 2 : 2 + tlen->len) + tagInfoSize;
}

size_t ASN1DERElement::payloadSize() const {
  size_t tagInfoSize = 0;
  if (((uint8_t *)_buf)[0] == 0xff) {
    ASN1PrivateTag *ptag = ((ASN1PrivateTag *)_buf) + 1;
    do {
      assert(_bufSize >= 2 + (++tagInfoSize));
    } while (ptag++->more);
  }

  size_t rt = 0;
  ASN1Len *tlen = (ASN1Len *)&_buf[1 + tagInfoSize];
  if (!tlen->isLong)
    return tlen->len;

  assert(tlen->len <= sizeof(size_t)); // can't hold more than size_t
  assert(_bufSize > 2 + tagInfoSize +
                        tlen->len); // len bytes shouldn't be outside of buffer

  for (uint8_t sizebits = 0; sizebits < tlen->len; sizebits++) {
    rt <<= 8;
    rt |= ((uint8_t *)_buf)[2 + tagInfoSize + sizebits];
  }

  return rt;
}

size_t ASN1DERElement::size() const { return taginfoSize() + payloadSize(); }

const void *ASN1DERElement::buf() const { return _buf; }

const void *ASN1DERElement::payload() const {
  return ((uint8_t *)_buf) + taginfoSize();
}

ASN1DERElement::ASN1TAG ASN1DERElement::tag() const {
  return *(ASN1TAG *)&_buf[0];
}

std::string ASN1DERElement::getStringValue() const {
  assert(((uint8_t *)_buf)[0] != 0xff);
  assert(_buf->tagNumber == TagNumber::TagIA5String ||
         _buf->tagNumber == TagNumber::TagOCTET);

  return {(char *)payload(), payloadSize()};
}

uint64_t ASN1DERElement::getIntegerValue() const {
  uint64_t rt = 0;
  assert(_buf->tagNumber == TagNumber::TagINTEGER ||
         _buf->tagNumber == TagNumber::TagBOOLEAN);
  assert(payloadSize() <= sizeof(uint64_t));
  for (uint8_t sizebits = 0; sizebits < payloadSize(); sizebits++) {
    rt <<= 8;
    rt |= ((uint8_t *)payload())[sizebits];
  }
  return rt;
}

void ASN1DERElement::print() const {
  switch (tag().tagNumber) {
  case TagIA5String:
    printf("%s", getStringValue().c_str());
    break;
  case TagOCTET: {
    std::string s = getStringValue();
    for (int i = 0; i < s.size(); i++) {
      printf("%02x", ((uint8_t *)s.c_str())[i]);
    }
    break;
  }
  case TagINTEGER:
    printf("%lu", getIntegerValue());
    break;
  case TagBOOLEAN:
    printf("%s", getIntegerValue() == 0 ? "false" : "true");
    break;
  default:
    printf("unimplemented ASN1DERElement::print() for type=%d",
           tag().tagNumber);
    break;
  }
}

ASN1DERElement ASN1DERElement::operator[](uint32_t i) const {
  assert(_buf->isConstructed);
  ASN1DERElement rt(payload(), payloadSize());

  while (i--) {
    uint8_t *buf = (uint8_t *)rt.buf() + rt.size();
    size_t size = _bufSize - (size_t)(buf - (uint8_t *)_buf);
    rt = ASN1DERElement(buf, size);
  }

  return rt;
}

ASN1DERElement &ASN1DERElement::operator+=(const ASN1DERElement &add) {
  assert(_buf->isConstructed && _ownsBuffer);

  std::string newSize = makeASN1Size(add.size() + payloadSize());

  if (newSize.size() < taginfoSize()) {
    // newSize fits in the buffer without resizing at front
    _buf =
        (const ASN1TAG *)realloc((void *)_buf, _bufSize = size() + add.size());

    memcpy((void *)&_buf[size()], add.buf(), add.size());
    memcpy((void *)&_buf[1], newSize.c_str(), newSize.size());
  } else {
    size_t size = add.size() + payloadSize() + 1 + newSize.size();
    const ASN1TAG *buf = (const ASN1TAG *)malloc(size);

    void *b = (void *)buf;
    buf = NULL;
    free(b);

    memcpy((void *)&buf[0], &_buf[0], 1);
    memcpy((void *)&buf[1], newSize.c_str(), newSize.size());
    memcpy((void *)&buf[1 + newSize.size()], payload(), payloadSize());
    memcpy((void *)&buf[1 + newSize.size() + payloadSize()], add.buf(),
           add.size());

    std::swap(_buf, buf);
    _bufSize = size;
  }

  return *this;
}

ASN1DERElement &ASN1DERElement::operator=(ASN1DERElement &&old) {
  if (_ownsBuffer) {
    void *buf = (void *)_buf;
    _buf = NULL;
    free(buf);
  }
  _buf = old._buf;
  _bufSize = old._bufSize;
  _ownsBuffer = old._ownsBuffer;
  old._ownsBuffer = false;

  return *this;
}

ASN1DERElement &ASN1DERElement::operator=(const ASN1DERElement &old) {
  _bufSize = old._bufSize;
  _ownsBuffer = true;
  _buf = (ASN1TAG *)malloc(_bufSize);
  memcpy((void *)&_buf[0], old._buf, _bufSize);

  return *this;
}

ASN1DERElement::ASN1DERElementIterator ASN1DERElement::begin() const {
  return {(const ASN1TAG *)payload(), payloadSize(), 0};
}

ASN1DERElement::ASN1DERElementIterator ASN1DERElement::end() const {
  return {(const ASN1TAG *)payload(), payloadSize(), payloadSize()};
}
