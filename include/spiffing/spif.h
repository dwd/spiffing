/***

Copyright 2014-2015 Dave Cridland
Copyright 2014-2015 Surevine Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

***/

#ifndef SPIFFING_SPIF_H
#define SPIFFING_SPIF_H

#include <string>
#include <map>
#include <istream>
#include <spiffing/constants.h>
#include <spiffing/classification.h>
#include <spiffing/marking.h>
#include <memory>

namespace Spiffing {
	class Spif {
	public:
		Spif(std::string const &, Format fmt);
		Spif(std::string &&, Format fmt);
		Spif(std::istream &, Format fmt);

		void parse(std::string const &, Format fmt);

		bool acdf(Label const &, Clearance const &) const;
		std::string displayMarking(Label const & l, MarkingCode location = MarkingCode::pageBottom) const {
			return displayMarking(l, "", location);
		}
		std::string displayMarking(Label const &, std::string const & langTag, MarkingCode location = MarkingCode::pageBottom) const;
		std::string displayMarking(Clearance const & c, std::string const & langTag) const;
		std::string displayMarking(Clearance const & c) const {
			return displayMarking(c, "");
		}
		bool valid(Label const & l) const;
		void assertValid(Label const & l) const;

		std::string const & policy_id() const {
			return m_oid;
		}

		std::string const & privilegeId() const {
			return m_privilegeId;
		}

		std::string const & rbacId() const {
			return m_rbacId;
		}

		[[nodiscard]] std::string const & name() const {
			return m_name;
		}

		[[nodiscard]] std::shared_ptr<TagSet> const & tagSetLookup(std::string_view const &) const;
        [[nodiscard]] std::shared_ptr<TagSet> const & tagSetLookupByName(std::string_view const &) const;
        [[nodiscard]] std::shared_ptr<Classification> const & classificationLookup(lacv_t cls) const {
			auto i = m_classifications.find(cls);
			if (i == m_classifications.end()) throw std::runtime_error("Unknown classification");
			return (*i).second;
		}
        [[nodiscard]] std::shared_ptr<Classification> const & classificationLookup(std::string_view const & clsv) const {
            std::string cls{clsv};
			auto i = m_classnames.find(cls);
			if (i == m_classnames.end()) throw std::runtime_error("Unknown classification");
			return (*i).second;
		}
		void encrypt(Label &) const;

	private:
		std::string m_name;
		std::string m_oid;
		std::string m_privilegeId; /* category syntax used in ASN clearances */
		std::string m_rbacId;      /* category syntax used in ASN labels */
		std::map<lacv_t,std::shared_ptr<Classification>> m_classifications;
		std::map<std::string,std::shared_ptr<Classification>> m_classnames;
		std::map<std::string /* OID */, std::shared_ptr<TagSet>> m_tagSets;
		std::map<std::string /* Name */, std::shared_ptr<TagSet>> m_tagSetsByName;
		std::map<std::string /* Name */, std::string /* OID */> m_equivPolicies;
		std::unique_ptr<Markings> m_markings;
		std::multimap<std::string /* OID */, std::unique_ptr<CategoryGroup>> m_equivReqs; /* Required Categories for equivalence */
	};
}

#endif
