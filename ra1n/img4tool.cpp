//
//  img4tool.cpp
//  img4tool
//
//  Created by tihmstar on 04.10.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//
#include "img4tool.hpp"
#include <array>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#endif
#include "ASN1DERElement.hpp"
//#include <img4tool/libgeneral/macros.h>
extern "C" {
#include "lzssdec.h"
};

#ifdef HAVE_LIBLZFSE
#include <lzfse.h>
#elif defined(HAVE_LIBCOMPRESSION)
#include <compression.h>
#define lzfse_decode_buffer(src, src_size, dst, dst_size, scratch)             \
  compression_decode_buffer(src, src_size, dst, dst_size, scratch,             \
                            COMPRESSION_LZFSE)
#endif

#ifdef HAVE_OPENSSL
#include <openssl/aes.h>
#include <openssl/sha.h>

#warning TODO adjust this for HAVE_COMMCRYPTO
#include <openssl/evp.h>  //not replaced by CommCrypto
#include <openssl/x509.h> //not replaced by CommCrypto
#else
#ifdef HAVE_COMMCRYPTO
#include <CommonCrypto/CommonCrypto.h>
#include <CommonCrypto/CommonDigest.h>
#define SHA1(d, n, md) CC_SHA1(d, n, md)
#define SHA384(d, n, md) CC_SHA384(d, n, md)
#define SHA_DIGEST_LENGTH CC_SHA1_DIGEST_LENGTH
#define SHA384_DIGEST_LENGTH CC_SHA384_DIGEST_LENGTH
#endif // HAVE_COMMCRYPTO
#endif // HAVE_OPENSSL

using namespace tihmstar::img4tool;

#define putStr(s, l) printf("%.*s", (int)l, s)

namespace tihmstar {
namespace img4tool {
void printKBAG(const void *buf, size_t size, std::vector<std::string> &KeyBags);
void printMANB(const void *buf, size_t size, bool printAll);
void printMANP(const void *buf, size_t size);

void printRecSequence(const void *buf, size_t size);

ASN1DERElement parsePrivTag(const void *buf, size_t size, size_t *outPrivTag);
ASN1DERElement unpackKernelIfNeeded(const ASN1DERElement &kernelOctet);
}; // namespace img4tool
}; // namespace tihmstar

#pragma mark private

void tihmstar::img4tool::printKBAG(const void *buf, size_t size,
                                   std::vector<std::string> &KeyBags) {
  ASN1DERElement octet(buf, size);

  assert(!octet.tag().isConstructed);
  assert(octet.tag().tagNumber == ASN1DERElement::TagOCTET);
  assert(octet.tag().tagClass == ASN1DERElement::TagClass::Universal);

  ASN1DERElement sequence(octet.payload(), octet.payloadSize());

  assert(sequence.tag().isConstructed);
  assert(sequence.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
  assert(sequence.tag().tagClass == ASN1DERElement::TagClass::Universal);

  printf("KBAG\n");
  for (auto &kbtag : sequence) {
    assert(kbtag.tag().isConstructed);
    assert(kbtag.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
    assert(kbtag.tag().tagClass == ASN1DERElement::TagClass::Universal);
    int i = -1;
    for (auto &elem : kbtag) {
      switch (++i) {
      case 0:
        printf("num: %lu\n", elem.getIntegerValue());
        break;
      case 1:
      case 2: {
        std::string kbagstr = elem.getStringValue();
        KeyBags.push_back(kbagstr);
        for (int i = 0; i < kbagstr.size(); i++) {
          printf("%02x", ((uint8_t *)kbagstr.c_str())[i]);
        }
        printf("\n");
        break;
      }
      default:
        printf("[%s] unexpected element at SEQUENCE index %d", __FUNCTION__, i);
        break;
      }
    }
  }
}

void tihmstar::img4tool::printMANB(const void *buf, size_t size,
                                   bool printAll) {
  
}

void tihmstar::img4tool::printMANP(const void *buf, size_t size) {
  
}

void tihmstar::img4tool::printRecSequence(const void *buf, size_t size) {
  
}


#pragma mark public

const char VERSION_STRING[] = "0.1_king";
const char *tihmstar::img4tool::version() { return VERSION_STRING; }

void tihmstar::img4tool::printIMG4(const void *buf, size_t size, bool printAll,
                                   bool im4pOnly,
                                   std::vector<std::string> &KeyBags) {
  ASN1DERElement sequence(buf, size);

  assert(sequence.tag().isConstructed);
  assert(sequence.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
  assert(sequence.tag().tagClass == ASN1DERElement::TagClass::Universal);

  ASN1DERElement filetype = sequence[0];

  assert(!filetype.tag().isConstructed);
  assert(filetype.tag().tagNumber == ASN1DERElement::TagIA5String);
  assert(filetype.tag().tagClass == ASN1DERElement::TagClass::Universal);

  {
    int i = -1;
    for (auto &tag : sequence) {
      switch (++i) {
        //#warning TODO we don't handle IM4R yet
      case 0:
        assert(tag.getStringValue() == "IMG4");
        printf("IMG4:\n");
        break;
      case 1:
        printIM4P(tag.buf(), tag.size(), KeyBags);
        break;
      case 2:
        if (!im4pOnly) {
          assert(tag.tag().isConstructed);
          assert(tag.tag().tagNumber == ASN1DERElement::TagEnd_of_Content);
          assert(tag.tag().tagClass ==
                 ASN1DERElement::TagClass::ContextSpecific);

          printIM4M(tag.payload(), tag.payloadSize(), printAll);
        }
        break;
      default:
        printf("[%s] unexpected element at SEQUENCE index %d", __FUNCTION__, i);
        break;
      }
    }
  }
}

void tihmstar::img4tool::printIM4P(const void *buf, size_t size,
                                   std::vector<std::string> &KeyBags) {
  ASN1DERElement sequence(buf, size);

  assert(sequence.tag().isConstructed);
  assert(sequence.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
  assert(sequence.tag().tagClass == ASN1DERElement::TagClass::Universal);

  {
    int i = -1;
    for (auto &tag : sequence) {
      switch (++i) {
      case 0:
        assert(tag.getStringValue() == "IM4P");
        printf("IM4P: ---------\n");
        break;
      case 1:
        assert(!tag.tag().isConstructed);
        assert(tag.tag().tagNumber == ASN1DERElement::TagIA5String);
        assert(tag.tag().tagClass == ASN1DERElement::TagClass::Universal);
        printf("type: %s\n", tag.getStringValue().c_str());
        break;
      case 2:
        assert(!tag.tag().isConstructed);
        assert(tag.tag().tagNumber == ASN1DERElement::TagIA5String);
        assert(tag.tag().tagClass == ASN1DERElement::TagClass::Universal);
        printf("desc: %s\n", tag.getStringValue().c_str());
        break;
      case 3:
        assert(!tag.tag().isConstructed);
        assert(tag.tag().tagNumber == ASN1DERElement::TagOCTET);
        assert(tag.tag().tagClass == ASN1DERElement::TagClass::Universal);
        printf("size: 0x%08lx\n\n", tag.payloadSize());
        break;
      case 4:
        printKBAG(tag.buf(), tag.size(), KeyBags);
        printf("\n");
        break;
      default:
        printf("[%s] unexpected element at SEQUENCE index %d", __FUNCTION__, i);
        break;
      }
    }
    if (i < 4) {
      printf("IM4P does not contain KBAG values\n\n");
    }
  }
}

void tihmstar::img4tool::printIM4M(const void *buf, size_t size,
                                   bool printAll) {
  ASN1DERElement sequence(buf, size);

  assert(sequence.tag().isConstructed);
  assert(sequence.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
  assert(sequence.tag().tagClass == ASN1DERElement::TagClass::Universal);

  {
    int i = -1;
    for (auto &tag : sequence) {
      switch (++i) {
      case 0:
        assert(tag.getStringValue() == "IM4M");
        printf("IM4M: ---------\n");
        break;
      case 1:
        printf("Version: %lu\n", tag.getIntegerValue());
        break;
      case 2:
        assert(tag.tag().isConstructed);
        assert(tag.tag().tagNumber == ASN1DERElement::TagSET);
        assert(tag.tag().tagClass == ASN1DERElement::TagClass::Universal);
        printMANB(tag.payload(), tag.payloadSize(), printAll);
        break;
      case 3: // signature
      case 4: // certificate
        break;
      default:
        printf("[%s] unexpected element at SEQUENCE index %d", __FUNCTION__, i);
        break;
      }
    }
  }
}

std::string tihmstar::img4tool::getNameForSequence(const void *buf,
                                                   size_t size) {
  ASN1DERElement sequence(buf, size);
  assert(sequence.tag().isConstructed);
  assert(sequence.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
  assert(sequence.tag().tagClass == ASN1DERElement::TagClass::Universal);

  return sequence[0].getStringValue();
}

ASN1DERElement tihmstar::img4tool::getIM4PFromIMG4(const ASN1DERElement &img4) {
  assert(isIMG4(img4));

  ASN1DERElement im4p = img4[1];
  assert(isIM4P(im4p));

  return im4p;
}

ASN1DERElement tihmstar::img4tool::getIM4MFromIMG4(const ASN1DERElement &img4) {
  assert(isIMG4(img4));

  ASN1DERElement container = img4[2];

  assert(container.tag().isConstructed);
  assert(container.tag().tagNumber == ASN1DERElement::TagEnd_of_Content);
  assert(container.tag().tagClass == ASN1DERElement::TagClass::ContextSpecific);

  ASN1DERElement im4m = container[0];

  assert(im4m.tag().isConstructed);
  assert(im4m.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
  assert(im4m.tag().tagClass == ASN1DERElement::TagClass::Universal);

  assert(im4m[0].getStringValue() == "IM4M" && "Container is not a IM4M");

  return im4m;
}

ASN1DERElement tihmstar::img4tool::getEmptyIMG4Container() {
  ASN1DERElement img4({ASN1DERElement::TagSEQUENCE, ASN1DERElement::Contructed,
                       ASN1DERElement::Universal},
                      NULL, 0);
  ASN1DERElement img4_str({ASN1DERElement::TagIA5String,
                           ASN1DERElement::Primitive,
                           ASN1DERElement::Universal},
                          "IMG4", 4);
  img4 += img4_str;

  return img4;
}

ASN1DERElement
tihmstar::img4tool::appendIM4PToIMG4(const ASN1DERElement &img4,
                                     const ASN1DERElement &im4p) {
  assert(isIMG4(img4));
  assert(isIM4P(im4p));

  ASN1DERElement newImg4(img4);

  newImg4 += im4p;

  return newImg4;
}

ASN1DERElement
tihmstar::img4tool::appendIM4MToIMG4(const ASN1DERElement &img4,
                                     const ASN1DERElement &im4m) {
  assert(isIMG4(img4));

  assert(im4m.tag().isConstructed);
  assert(im4m.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
  assert(im4m.tag().tagClass == ASN1DERElement::TagClass::Universal);

  assert(im4m[0].getStringValue() == "IM4M" && "Container is not a IM4P");

  ASN1DERElement newImg4(img4);

  ASN1DERElement container({ASN1DERElement::TagEnd_of_Content,
                            ASN1DERElement::Contructed,
                            ASN1DERElement::ContextSpecific},
                           NULL, 0);

  container += im4m;

  newImg4 += container;

  return newImg4;
}

ASN1DERElement
tihmstar::img4tool::unpackKernelIfNeeded(const ASN1DERElement &kernelOctet) {
  const char *payload = (const char *)kernelOctet.payload();
  size_t unpackedLen = 0;
  char *unpacked = NULL;
  free(unpacked);
  ASN1DERElement retVal = kernelOctet;

  if (strncmp(payload, "complzss", 8) == 0) {
    printf("Kernelcache detected, uncompressing (%s): ", "complzss");
    if ((unpacked = tryLZSS(payload, &unpackedLen))) {
      retVal =
          ASN1DERElement({ASN1DERElement::TagNumber::TagOCTET,
                          ASN1DERElement::Primitive, ASN1DERElement::Universal},
                         unpacked, unpackedLen);
      printf("ok\n");
    } else {
      printf("failed!\n");
    }
  } else if (strncmp(payload, "bvx2", 4) == 0) {
    printf("Kernelcache detected, uncompressing (%s): ", "bvx2");
    printf("failed!\n");
    //#warning TODO implement bvx2
    printf("Unpacking bvx2 currently not implemented!\n");
  }

  return retVal;
}

ASN1DERElement tihmstar::img4tool::getPayloadFromIM4P(
    const ASN1DERElement &im4p, const char *decryptIv, const char *decryptKey) {
  assert(isIM4P(im4p));
  ASN1DERElement payload = im4p[3];
  if (decryptIv || decryptKey) {
#ifdef HAVE_CRYPTO
    payload = decryptPayload(payload, decryptIv, decryptKey);
#else
    printf("decryption keys were provided, but img4tool was compiled without "
           "crypto backend!");
#endif // HAVE_CRYPTO
  }

  return unpackKernelIfNeeded(payload);
}

#pragma mark begin_needs_crypto
#ifdef HAVE_CRYPTO
ASN1DERElement tihmstar::img4tool::decryptPayload(const ASN1DERElement &payload,
                                                  const char *decryptIv,
                                                  const char *decryptKey) {
  uint8_t iv[16] = {};
  uint8_t key[32] = {};
  printf(decryptIv, "decryptPayload requires IV but none was provided!");
  printf(decryptKey, "decryptPayload requires KEY but none was provided!");

  assert(!payload.tag().isConstructed);
  assert(payload.tag().tagNumber == ASN1DERElement::TagOCTET);
  assert(payload.tag().tagClass == ASN1DERElement::TagClass::Universal);

  ASN1DERElement decPayload(payload);

  assert(strlen(decryptIv) == sizeof(iv) * 2);
  assert(strlen(decryptKey) == sizeof(key) * 2);
  for (int i = 0; i < sizeof(iv); i++) {
    unsigned int t;
    assert(sscanf(decryptIv + i * 2, "%02x", &t) == 1);
    iv[i] = t;
  }
  for (int i = 0; i < sizeof(key); i++) {
    unsigned int t;
    assert(sscanf(decryptKey + i * 2, "%02x", &t) == 1);
    key[i] = t;
  }

#ifdef HAVE_OPENSSL
  AES_KEY decKey = {};
  printf(!AES_set_decrypt_key(key, sizeof(key) * 8, &decKey),
         "Failed to set decryption key");
  AES_cbc_encrypt((const unsigned char *)decPayload.payload(),
                  (unsigned char *)decPayload.payload(),
                  decPayload.payloadSize(), &decKey, iv, AES_DECRYPT);
#else
#ifdef HAVE_COMMCRYPTO
  printf(CCCrypt(kCCDecrypt, kCCAlgorithmAES, 0, key, sizeof(key), iv,
                 decPayload.payload(), decPayload.payloadSize(),
                 (void *)decPayload.payload(), decPayload.payloadSize(),
                 NULL) == kCCSuccess,
         "Decryption failed!");
#endif // HAVE_COMMCRYPTO
#endif // HAVE_OPENSSL

  return decPayload;
}

std::string tihmstar::img4tool::getIM4PSHA1(const ASN1DERElement &im4p) {
  std::array<char, SHA_DIGEST_LENGTH> tmp{'\0'};
  std::string hash{tmp.begin(), tmp.end()};
  SHA1((unsigned char *)im4p.buf(), (unsigned int)im4p.size(),
       (unsigned char *)hash.c_str());
  return hash;
}

std::string tihmstar::img4tool::getIM4PSHA384(const ASN1DERElement &im4p) {
  std::array<char, SHA384_DIGEST_LENGTH> tmp{'\0'};
  std::string hash{tmp.begin(), tmp.end()};
  SHA384((unsigned char *)im4p.buf(), (unsigned int)im4p.size(),
         (unsigned char *)hash.c_str());
  return hash;
}

bool tihmstar::img4tool::im4mContainsHash(const ASN1DERElement &im4m,
                                          std::string hash) {
  assert(isIM4M(im4m));
  ASN1DERElement set = im4m[2];
  ASN1DERElement manbpriv = set[0];
  size_t privTagVal = 0;
  ASN1DERElement manb =
      parsePrivTag(manbpriv.buf(), manbpriv.size(), &privTagVal);
  assert(privTagVal == *(uint32_t *)"MANB");
  assert(manb[0].getStringValue() == "MANB");

  ASN1DERElement manbset = manb[1];

  for (auto &e : manbset) {
    size_t pTagVal = 0;
    ASN1DERElement me = parsePrivTag(e.buf(), e.size(), &pTagVal);
    if (pTagVal == *(uint32_t *)"MANP")
      continue;

    ASN1DERElement set = me[1];
    auto asd = me[0].getStringValue();

    for (auto &se : set) {
      size_t pTagVal = 0;
      ASN1DERElement sel = parsePrivTag(se.buf(), se.size(), &pTagVal);
      switch (pTagVal) {
      case 'TSGD': // DGST
      {
        std::string selDigest = sel[1].getStringValue();
        if (selDigest == hash) {
          return true;
        }
      } break;
      default:
        break;
      }
    }
  }
  return false;
}
#endif // HAVE_CRYPTO
#pragma mark end_needs_crypto

ASN1DERElement tihmstar::img4tool::getEmptyIM4PContainer(const char *type,
                                                         const char *desc) {
  ASN1DERElement im4p({ASN1DERElement::TagSEQUENCE, ASN1DERElement::Contructed,
                       ASN1DERElement::Universal},
                      NULL, 0);
  ASN1DERElement im4p_str({ASN1DERElement::TagIA5String,
                           ASN1DERElement::Primitive,
                           ASN1DERElement::Universal},
                          "IM4P", 4);
  ASN1DERElement im4p_type({ASN1DERElement::TagIA5String,
                            ASN1DERElement::Primitive,
                            ASN1DERElement::Universal},
                           type, strlen(type));
  ASN1DERElement im4p_desc({ASN1DERElement::TagIA5String,
                            ASN1DERElement::Primitive,
                            ASN1DERElement::Universal},
                           desc, strlen(desc));

  assert(im4p_type.payloadSize() == 4 &&
         "Type needs to be exactly 4 bytes long");

  im4p += im4p_str;
  im4p += im4p_type;
  im4p += im4p_desc;

  return im4p;
}

ASN1DERElement
tihmstar::img4tool::appendPayloadToIM4P(const ASN1DERElement &im4p,
                                        const void *buf, size_t size) {
  assert(im4p.tag().isConstructed);
  assert(im4p.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
  assert(im4p.tag().tagClass == ASN1DERElement::TagClass::Universal);

  assert(im4p[0].getStringValue() == "IM4P" && "Container is not a IM4P");
  assert(im4p[1].getStringValue().size() == 4 && "IM4P type has size != 4");
  assert(im4p[2].getStringValue().size() && "IM4P description is empty");
  ASN1DERElement newim4p(im4p);

  ASN1DERElement im4p_payload({ASN1DERElement::TagOCTET,
                               ASN1DERElement::Primitive,
                               ASN1DERElement::Universal},
                              buf, size);

  newim4p += im4p_payload;

  return newim4p;
}

bool tihmstar::img4tool::isIMG4(const ASN1DERElement &img4) {
  try {
    assert(img4.tag().isConstructed);
    assert(img4.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
    assert(img4.tag().tagClass == ASN1DERElement::TagClass::Universal);

    assert(img4[0].getStringValue() == "IMG4" && "Not an IMG4 file");
    return true;
  } catch (std::exception &e) {
    //
  }
  return false;
}

bool tihmstar::img4tool::isIM4P(const ASN1DERElement &im4p) {
  try {
    assert(im4p.tag().isConstructed);
    assert(im4p.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
    assert(im4p.tag().tagClass == ASN1DERElement::TagClass::Universal);

    assert(im4p[0].getStringValue() == "IM4P" && "Container is not a IM4P");
    assert(im4p[1].getStringValue().size() == 4 && "IM4P type has size != 4");
    assert(im4p[2].getStringValue().size() && "IM4P description is empty");

    ASN1DERElement payload = im4p[3];
    assert(!payload.tag().isConstructed);
    assert(payload.tag().tagNumber == ASN1DERElement::TagOCTET);
    assert(payload.tag().tagClass == ASN1DERElement::TagClass::Universal);

    return true;
  } catch (std::exception &e) {
    //
  }
  return false;
}

bool tihmstar::img4tool::isIM4M(const ASN1DERElement &im4m) {
  try {
    assert(im4m.tag().isConstructed);
    assert(im4m.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
    assert(im4m.tag().tagClass == ASN1DERElement::TagClass::Universal);

    assert(im4m[0].getStringValue() == "IM4M" && "Container is not a IM4M");
    assert(im4m[1].getIntegerValue() == 0 && "IM4M has weird version number");

    auto set = im4m[2];
    assert(set.tag().isConstructed);
    assert(set.tag().tagNumber == ASN1DERElement::TagSET);
    assert(set.tag().tagClass == ASN1DERElement::TagClass::Universal);

    auto octet = im4m[3];
    assert(!octet.tag().isConstructed);
    assert(octet.tag().tagNumber == ASN1DERElement::TagOCTET);
    assert(octet.tag().tagClass == ASN1DERElement::TagClass::Universal);

    auto seq = im4m[4];
    assert(seq.tag().isConstructed);
    assert(seq.tag().tagNumber == ASN1DERElement::TagSEQUENCE);
    assert(seq.tag().tagClass == ASN1DERElement::TagClass::Universal);

    return true;
  } catch (std::exception &e) {
    //
  }
  return false;
}

ASN1DERElement tihmstar::img4tool::renameIM4P(const ASN1DERElement &im4p,
                                              const char *type) {
  assert(isIM4P(im4p));
  assert(strlen(type) == 4 && "type has size != 4");
  ASN1DERElement newIm4p(im4p);

  memcpy((void *)newIm4p[1].payload(), type, 4);

  return newIm4p;
}

#pragma mark begin_needs_openssl
#ifdef HAVE_OPENSSL
bool tihmstar::img4tool::isIM4MSignatureValid(const ASN1DERElement &im4m) {

  EVP_MD_CTX *mdctx = NULL;
  X509 *cert = NULL;
  EVP_PKEY *certpubkey = NULL;
  const unsigned char *certificate = NULL;
  bool useSHA384 = false;
  cleanup([&] {
    if (mdctx)
      EVP_MD_CTX_destroy(mdctx);
  });

  try {
    assert(isIM4M(im4m));
    ASN1DERElement data = im4m[2];
    ASN1DERElement sig = im4m[3];
    ASN1DERElement certelem = im4m[4][0];
    try {
      // bootAuthority is 0
      // tssAuthority is 1
      certelem = im4m[4][1];
    } catch (tihmstar::exception &e) {
      // however bootAuthority does not exist on iPhone7
      useSHA384 = true;
    }

    certificate = (const unsigned char *)certelem.buf();

    assert(mdctx = EVP_MD_CTX_create());
    assert(cert = d2i_X509(NULL, &certificate, certelem.size()));
    assert(certpubkey = X509_get_pubkey(cert));

    assert(EVP_DigestVerifyInit(mdctx, NULL,
                                (useSHA384) ? EVP_sha384() : EVP_sha1(), NULL,
                                certpubkey) == 1);

    assert(EVP_DigestVerifyUpdate(mdctx, data.buf(), data.size()) == 1);

    assert(EVP_DigestVerifyFinal(mdctx, (unsigned char *)sig.payload(),
                                 sig.payloadSize()) == 1);
  } catch (tihmstar::exception &e) {
    printf("[IMG4TOOL] failed to verify IM4M signature with error:\n");
    e.dump();
    return false;
  }
  return true;
}
#endif // HAVE_OPENSSL
#pragma mark end_needs_openssl

#pragma mark begin_needs_plist
#ifdef HAVE_PLIST
bool tihmstar::img4tool::doesIM4MBoardMatchBuildIdentity(
    const ASN1DERElement &im4m, plist_t buildIdentity) noexcept {
  plist_t ApBoardID = NULL;
  plist_t ApChipID = NULL;
  plist_t ApSecurityDomain = NULL;
  try {
    assert(isIM4M(im4m));

    assert(ApBoardID = plist_dict_get_item(buildIdentity, "ApBoardID"));
    assert(ApChipID = plist_dict_get_item(buildIdentity, "ApChipID"));
    assert(ApSecurityDomain =
               plist_dict_get_item(buildIdentity, "ApSecurityDomain"));

    assert(plist_get_node_type(ApBoardID) == PLIST_STRING);
    assert(plist_get_node_type(ApChipID) == PLIST_STRING);
    assert(plist_get_node_type(ApSecurityDomain) == PLIST_STRING);

    ASN1DERElement set = im4m[2];
    ASN1DERElement manbpriv = set[0];
    size_t privTagVal = 0;
    ASN1DERElement manb =
        parsePrivTag(manbpriv.buf(), manbpriv.size(), &privTagVal);
    assert(privTagVal == *(uint32_t *)"MANB");
    assert(manb[0].getStringValue() == "MANB");

    ASN1DERElement manbset = manb[1];

    ASN1DERElement manppriv = manbset[0];
    privTagVal = 0;
    ASN1DERElement manp =
        parsePrivTag(manppriv.buf(), manppriv.size(), &privTagVal);
    assert(privTagVal == *(uint32_t *)"MANP");
    assert(manp[0].getStringValue() == "MANP");

    ASN1DERElement manpset = manp[1];

    for (auto &e : manpset) {
      char *pstrval = NULL;
      uint64_t val = 0;
      size_t ptagVal = 0;
      plist_t currVal = NULL;
      cleanup([&] { safeFree(pstrval); });
      ASN1DERElement ptag = parsePrivTag(e.buf(), e.size(), &ptagVal);

      switch (ptagVal) {
      case 'DROB': // BORD
        assert(ptag[0].getStringValue() == "BORD");
        currVal = ApBoardID;
        ApBoardID = NULL;
        break;
      case 'PIHC': // CHIP
        assert(ptag[0].getStringValue() == "CHIP");
        currVal = ApChipID;
        ApChipID = NULL;
        break;
      case 'MODS': // SDOM
        assert(ptag[0].getStringValue() == "SDOM");
        currVal = ApSecurityDomain;
        ApSecurityDomain = NULL;
        break;
      default:
        continue;
      }

      plist_get_string_val(currVal, &pstrval);
      if (strncmp("0x", pstrval, 2) == 0) {
        sscanf(pstrval, "0x%lx", &val);
      } else {
        sscanf(pstrval, "%ld", &val);
      }
      assert(ptag[1].getIntegerValue() == val);
    }
    // make sure we verified all 3 values we wanted to check
    assert(!ApBoardID && !ApChipID && !ApSecurityDomain);
  } catch (...) {
    return false;
  }
  return true;
}

bool tihmstar::img4tool::im4mMatchesBuildIdentity(
    const ASN1DERElement &im4m, plist_t buildIdentity) noexcept {
  plist_t manifest = NULL;
  try {
    printf("[IMG4TOOL] checking buildidentity matches board ... ");
    if (!doesIM4MBoardMatchBuildIdentity(im4m, buildIdentity)) {
      printf("NO\n");
      return false;
    }
    printf("YES\n");

    printf("[IMG4TOOL] checking buildidentity has all required hashes:\n");
    ASN1DERElement set = im4m[2];
    ASN1DERElement manbpriv = set[0];
    size_t privTagVal = 0;
    ASN1DERElement manb =
        parsePrivTag(manbpriv.buf(), manbpriv.size(), &privTagVal);
    assert(privTagVal == *(uint32_t *)"MANB");
    assert(manb[0].getStringValue() == "MANB");

    ASN1DERElement manbset = manb[1];

    assert(manifest = plist_dict_get_item(buildIdentity, "Manifest"));
    assert(plist_get_node_type(manifest) == PLIST_DICT);

    plist_dict_iter melems = NULL;
    cleanup([&] { safeFree(melems); });
    plist_dict_new_iter(manifest, &melems);
    assert(melems);
    plist_t eVal = NULL;
    char *eKey = NULL;

    while (((void)plist_dict_next_item(manifest, melems, &eKey, &eVal), eVal)) {
      plist_t pInfo = NULL;
      plist_t pDigest = NULL;
      plist_t pPersonalize = NULL;
      uint8_t doPersonalize = 0;
      char *digest = NULL;
      uint64_t digestLen = 0;
      bool hasDigit = false;
      cleanup([&] { safeFree(digest); });

      int didprint = printf("[IMG4TOOL] checking hash for \"%s\"", eKey);
      while (didprint++ < 55) {
        printf(" ");
      }

      assert(pInfo = plist_dict_get_item(eVal, "Info"));
      if (!(pPersonalize = plist_dict_get_item(pInfo, "Personalize"))) {
        printf("OK (unchecked)\n");
        continue;
      }
      assert(plist_get_node_type(pPersonalize) == PLIST_BOOLEAN);
      plist_get_bool_val(pPersonalize, &doPersonalize);
      if (!doPersonalize) {
        printf("OK (unpersonalized)\n");
        continue;
      }
      assert(pDigest = plist_dict_get_item(eVal, "Digest"));
      assert(plist_get_node_type(pDigest) == PLIST_DATA);
      plist_get_data_val(pDigest, &digest, &digestLen);

      for (auto &e : manbset) {
        size_t pTagVal = 0;
        ASN1DERElement me = parsePrivTag(e.buf(), e.size(), &pTagVal);
        if (pTagVal == *(uint32_t *)"MANP")
          continue;

        ASN1DERElement set = me[1];

        for (auto &se : set) {
          size_t pTagVal = 0;
          ASN1DERElement sel = parsePrivTag(se.buf(), se.size(), &pTagVal);
          switch (pTagVal) {
          case 'TSGD': // DGST
          {
            std::string selDigest = sel[1].getStringValue();
            if (selDigest.size() == digestLen &&
                memcmp(selDigest.c_str(), digest, digestLen) == 0) {
              hasDigit = true;
              printf("OK (found \"%s\" with matching hash)\n",
                     me[0].getStringValue().c_str());
              goto continue_plist;
            }
          } break;
          default:
            break;
          }
        }
      }
    continue_plist:
      assert(hasDigit);
    }
  } catch (tihmstar::exception &e) {
    printf("\nfailed verification with error:\n");
    e.dump();
    return false;
  }
  return true;
}

const plist_t
tihmstar::img4tool::getBuildIdentityForIm4m(const ASN1DERElement &im4m,
                                            plist_t buildmanifest) {
  plist_t buildidentities = NULL;

  assert(buildmanifest);
  assert(buildidentities =
             plist_dict_get_item(buildmanifest, "BuildIdentities"));
  assert(plist_get_node_type(buildidentities) == PLIST_ARRAY);

  for (int i = 0; i < plist_array_get_size(buildidentities); i++) {
    plist_t buildIdentity = NULL;

    printf("[IMG4TOOL] checking buildidentity %d:\n", i);
    assert(buildIdentity = plist_array_get_item(buildidentities, i));
    if (im4mMatchesBuildIdentity(im4m, buildIdentity)) {
      return buildIdentity;
    }
  }
  reterror("Failed to find matching buildidentity");
}

void tihmstar::img4tool::printGeneralBuildIdentityInformation(
    plist_t buildidentity) {
  plist_t info = NULL;
  plist_dict_iter iter = NULL;
  cleanup([&] { safeFree(iter); });
  assert(info = plist_dict_get_item(buildidentity, "Info"));

  assert(((void)plist_dict_new_iter(info, &iter), iter));

  plist_t node = NULL;
  char *key = NULL;
  while ((void)plist_dict_next_item(info, iter, &key, &node), node) {
    char *str = NULL;
    cleanup([&] { safeFree(str); });
    plist_type t = PLIST_NONE;
    switch (t = plist_get_node_type(node)) {
    case PLIST_STRING:
      plist_get_string_val(node, &str);
      printf("%s : %s\n", key, str);
      break;
    case PLIST_BOOLEAN:
      plist_get_bool_val(node, (uint8_t *)&t);
      printf("%s : %s\n", key, ((uint8_t)t) ? "YES" : "NO");
    default:
      break;
    }
  }
}

bool tihmstar::img4tool::isValidIM4M(const ASN1DERElement &im4m,
                                     plist_t buildmanifest) {
  try {
    plist_t buildIdentity = NULL;
    buildIdentity = getBuildIdentityForIm4m(im4m, buildmanifest);

#ifdef HAVE_OPENSSL
    if (!isIM4MSignatureValid(im4m)) {
      reterror("Signature verification of IM4M failed!\n");
    }
    printf("\n[IMG4TOOL] IM4M signature is verified by TssAuthority\n");
#else
    printf("[WARNING] COMPILED WITHOUT OPENSSL: can not im4m signature!\n");
#endif // HAVE_OPENSSL

    printf("[IMG4TOOL] IM4M is valid for the given BuildManifest for the "
           "following restore:\n");
    printGeneralBuildIdentityInformation(buildIdentity);
  } catch (tihmstar::exception &e) {
    printf("\n[IMG4TOOL] IM4M validation failed with error:\n");
    e.dump();
    return false;
  }

  return true;
}
#endif // HAVE_PLIST
#pragma mark end_needs_plist
