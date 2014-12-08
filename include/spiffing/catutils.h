#ifndef SPIFFING_CATUTILS_H
#define SPIFFING_CATUTILS_H

#include <spiffing/constants.h>
#include <spiffing/category.h>
#include <spiffing/categoryref.h>
#include <string>
#include <ANY.h>
#include <OBJECT_IDENTIFIER.h>
#include <EnumeratedTag.h>
#include <SecurityCategories.h>
#include <stdexcept>
#include <set>

namespace Spiffing {



  template<typename ASN_t> class Asn {
  public:
    Asn(asn_TYPE_descriptor_t * t) : m_def(t), m_obj(nullptr) {};
    ASN_t & operator*() { return *m_obj; }
    ASN_t const & operator*() const { return *m_obj; }
    ASN_t * operator -> () { return m_obj; }
    ASN_t const * operator -> () const { return m_obj; }
    void ** addr() { return reinterpret_cast<void **>(&m_obj); }
    ~Asn() { if (m_obj) m_def->free_struct(m_def, m_obj, 0); }

    void alloc() { m_obj = (ASN_t*)calloc(1, sizeof(ASN_t)); }
    ASN_t * release() {
      ASN_t * tmp{m_obj};
      m_obj = nullptr;
      return tmp;
    }
  private:
    asn_TYPE_descriptor_t * const m_def;
    ASN_t * m_obj;
  };

  namespace Internal {

    int write_to_string(const void * buffer, size_t size, void * app_key);

    std::string oid2str(OBJECT_IDENTIFIER_t * oid);
    OBJECT_IDENTIFIER_t * str2oid(std::string const & s);
    void str2oid(std::string const & s, OBJECT_IDENTIFIER_t *);

    SecurityCategories * catencode(std::set<CategoryRef> const &);

    template<typename Object>
    void parse_enum_cat(TagType enum_type, Object & object, ANY * any) {
      Asn<EnumeratedTag_t> enumeratedTag(&asn_DEF_EnumeratedTag);
      auto r = ANY_to_type(any, &asn_DEF_EnumeratedTag, enumeratedTag.addr());
      if (r != RC_OK) {
        throw std::runtime_error("Failed to decode enumerated tag");
      }
      std::string tagSetName = oid2str(&enumeratedTag->tagName);
      for (size_t ii{0}; ii != enumeratedTag->attributeList.list.count; ++ii) {
        auto n = enumeratedTag->attributeList.list.array[ii];
        Lacv catLacv{std::string((char *)n->buf, n->size)};
        auto cat = object.policy().tagSetLookup(tagSetName)->categoryLookup(enum_type, catLacv);
        object.addCategory(cat);
      }
    }

    template<typename Object, typename Tag>
    void parse_cat(TagType type, asn_TYPE_descriptor_t * asn_def, Object & object, ANY * any) {
      Asn<Tag> tag(asn_def);
      auto r = ANY_to_type(any, asn_def, tag.addr());
      if (r != RC_OK) {
        throw std::runtime_error("Failed to decode attributeFlag tag");
      }
      std::string tagSetName = oid2str(&tag->tagName);
      for (size_t ii{0}; ii != tag->attributeFlags.size; ++ii) {
        if (!tag->attributeFlags.buf[ii]) continue;
        for (int ij{0}; ij != 8; ++ij) {
          if (tag->attributeFlags.buf[ii] & (1 << (7 - ij))) {
            Lacv catLacv{(ii*8 + ij)};
            auto cat = object.policy().tagSetLookup(tagSetName)->categoryLookup(type, catLacv);
            object.addCategory(cat);
          }
        }
      }
    }
  }

}

#endif