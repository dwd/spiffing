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

#include <spiffing/spif.h>
#include <rapidxml.hpp>
#include <spiffing/label.h>
#include <spiffing/clearance.h>
#include <spiffing/tagset.h>
#include <spiffing/tag.h>
#include <spiffing/category.h>
#include <spiffing/lacv.h>
#include <INTEGER.h>
#include <spiffing/catutils.h>
#include <spiffing/marking.h>
#include <spiffing/categorydata.h>
#include <spiffing/categorygroup.h>
#include <spiffing/equivclass.h>
#include <spiffing/equivcat.h>
#include <tuple>
#include <spiffing/exceptions.h>

using namespace Spiffing;

Spif::Spif(std::string const & s, Format fmt)
: m_classifications() {
	parse(s, fmt);
}

Spif::Spif(std::istream & s, Format fmt)
: m_classifications() {
	std::string data{std::istreambuf_iterator<char>(s), std::istreambuf_iterator<char>()};
	parse(data, fmt);
}

namespace {
	Lacv parseLacv(rapidxml::xml_attribute<>::ptr lacv) {
		return Lacv::parse(lacv->value());
	}

	std::unique_ptr<Markings> parseMarkings(rapidxml::xml_node<>::ptr holder) {
		std::set<std::string> langTags; // Previously processed tags.
		std::unique_ptr<Markings> markings;
		for (;;) {
			std::unique_ptr<Marking> ptr;
			bool found{false}; // Have we found a new language tag to process?
			std::string langTag; // Current tag
			for (auto qual = holder->first_node("markingQualifier"); qual; qual = qual->next_sibling(
					"markingQualifier")) {
				for (auto q = qual->first_node("qualifier"); q; q = q->next_sibling("qualifier")) {
					if (auto lt_attr = q->first_attribute("xml:lang")) {
						std::string tag{lt_attr->value()};
						if (found) {
							if (langTag != lt_attr->value()) {
								continue; // Not yet.
							}
						} else {
							if (langTags.find(tag) == langTags.end()) {
								langTags.insert(tag);
								found = true;
								langTag = tag;
							} else {
								continue; // Done already.
							}
						}
					} else {
						// No language tag set.
						if (found) {
							if (!langTag.empty()) {
								continue;
							}
						} else {
							if (langTags.find("") == langTags.end()) {
								langTags.insert("");
								found = true;
							} else {
								continue;
							}
						}
					}
					auto qcode_a = q->first_attribute("qualifierCode");
					auto txt_a = q->first_attribute("markingQualifier");
					if (!txt_a) continue;
					if (qcode_a) {
						if (!ptr) ptr = std::make_unique<Marking>(langTag);
						if (qcode_a->value() == "prefix") {
							ptr->prefix(txt_a->value());
						} else if (qcode_a->value() == "suffix") {
							ptr->suffix(txt_a->value());
						} else if (qcode_a->value() == "separator") {
							ptr->sep(txt_a->value());
						}
					}
				}
				// Second pass; backfill defaults.
				if (found && ptr) {
					for (auto q = qual->first_node("qualifier"); q; q = q->next_sibling("qualifier")) {
						if (q->first_attribute("xml:lang")) {
							// Skip over any lang-qualified qualifiers.
							continue;
						}
						auto qcode_a = q->first_attribute("qualifierCode");
						auto txt_a = q->first_attribute("markingQualifier");
						if (!txt_a) continue;
						if (qcode_a) {
							if (qcode_a->value() == "prefix") {
								if (ptr->prefix().empty()) ptr->prefix(txt_a->value());
							} else if (qcode_a->value() == "suffix") {
								if (ptr->suffix().empty()) ptr->suffix(txt_a->value());
							} else if (qcode_a->value() == "separator") {
								if (ptr->sep().empty()) ptr->sep(txt_a->value());
							}
						}
					}
				}
			}
			for (auto data = holder->first_node("markingData"); data; data = data->next_sibling("markingData")) {
				if (auto lt_attr = data->first_attribute("xml:lang")) {
					std::string tag{lt_attr->value()};
					if (found) {
						if (langTag != lt_attr->value()) {
							continue; // Not yet.
						}
					} else {
						if (langTags.find(tag) == langTags.end()) {
							found = true;
							langTag = tag;
							langTags.insert(tag);
						} else {
							continue; // Done already.
						}
					}
				} else {
					// No language tag set.
					if (found) {
						if (langTag != "") {
							continue;
						}
					} else {
						if (langTags.find("") == langTags.end()) {
							found = true;
							langTags.insert("");
						} else {
							continue;
						}
					}
				}
				auto phrase_a = data->first_attribute("phrase");
				if (!ptr) ptr = std::make_unique<Marking>(langTag);
				int loc(0);
				bool locset = false;
				for (auto code = data->first_node("code"); code; code = code->next_sibling("code")) {
					if (code->value() == "noMarkingDisplay") {
						loc |= MarkingCode::noMarkingDisplay;
						locset = true;
					} else if (code->value() == "noNameDisplay") {
						loc |= MarkingCode::noNameDisplay;
					} else if (code->value() == "suppressClassName") {
						loc |= MarkingCode::suppressClassName;
					} else if (code->value() == "pageBottom") {
						loc |= MarkingCode::pageBottom;
						locset = true;
					} else if (code->value() == "pageTop") {
						loc |= MarkingCode::pageTop;
						locset = true;
					} else if (code->value() == "pageTopBottom") {
						loc |= MarkingCode::pageTop | MarkingCode::pageBottom;
						locset = true;
					} else if (code->value() == "replacePolicy") {
						loc |= MarkingCode::replacePolicy;
					} else if (code->value() == "documentStart") {
						loc |= MarkingCode::documentStart;
						locset = true;
					} else if (code->value() == "documentEnd") {
						loc |= MarkingCode::documentEnd;
						locset = true;
					} else {
						throw spif_syntax_error("Unknown marking code");
					}
				}
				if (!locset) {
					loc |= MarkingCode::pageBottom;
				}
				std::optional<std::string> phrase;
				if (phrase_a) {
					phrase.emplace(phrase_a->value());
				}
				ptr->addPhrase(loc, phrase);
			}
			if (found) {
				if (!markings) markings = std::make_unique<Markings>();
				markings->marking(std::move(ptr));
			} else {
				return markings;
			}
		}
	}

	TagType parseTagType(rapidxml::xml_node<>::ptr node) {
		auto tagType_a = node->first_attribute("tagType");
		if (!tagType_a) throw spif_syntax_error("element has no tagType");
		TagType tagType;
		if (tagType_a->value() == "permissive") {
			tagType = TagType::permissive;
		} else if (tagType_a->value() == "restrictive") {
			tagType = TagType::restrictive;
		} else if (tagType_a->value() == "enumerated") {
			auto enumType_a = node->first_attribute("enumType");
			if (!enumType_a) throw spif_syntax_error("element has no enumType");
			if (enumType_a->value() == "permissive") {
				tagType = TagType::enumeratedPermissive;
			} else {
				tagType = TagType::enumeratedRestrictive;
			}
		} else {
			tagType = TagType::informative;
		}
		return tagType;
	}

	// For excludedCategory, also used within categoryGroup.
	std::unique_ptr<CategoryData> parseOptionalCategoryData(rapidxml::xml_node<>::ptr node) {
		std::string tagSetRef;
		auto tagSetRef_a = node->first_attribute("tagSetRef");
		if (tagSetRef_a) {
			tagSetRef = tagSetRef_a->value();
		} else {
			throw spif_syntax_error("Missing tagSetRef in OptionalCategoryData");
		}
		TagType tagType = parseTagType(node);
		auto lacv_a = node->first_attribute("lacv");
		if (lacv_a) {
			Lacv l{parseLacv(lacv_a)};
			return std::make_unique<CategoryData>(tagSetRef, tagType, l);
		} else {
			return std::make_unique<CategoryData>(tagSetRef, tagType);
		}
	}

	// For requiredCategory
	std::unique_ptr<CategoryGroup> parseCategoryGroup(rapidxml::xml_node<>::ptr node) {
		auto op_a = node->first_attribute("operation");
		if (!op_a || op_a->value().empty()) throw spif_syntax_error("Missing operation attribute from CategoryGroup");
		OperationType opType{OperationType::onlyOne};
		if (op_a->value() == "onlyOne") {
			opType = OperationType::onlyOne;
		} else if (op_a->value() == "oneOrMore") {
			opType = OperationType::oneOrMore;
		} else if (op_a->value() == "all") {
			opType = OperationType::all;
		}
		std::unique_ptr<CategoryGroup> group = std::make_unique<CategoryGroup>(opType);
		for (auto cd = node->first_node("categoryGroup"); cd; cd = cd->next_sibling("categoryGroup")) {
			group->addCategoryData(parseOptionalCategoryData(cd));
		}
		return group;
	}

	template<typename T>
	void parseExcludedClass(rapidxml::xml_node<>::ptr node, T & t, std::map<std::string,std::shared_ptr<Classification>> const & classNames) {
		for (auto excClass = node->first_node("excludedClass"); excClass; excClass = excClass->next_sibling("excludedClass")) {
			if (excClass->value().empty()) throw spif_syntax_error("Empty excludedClass element");
			std::string className{excClass->value()};
			auto it = classNames.find(className);
			if (it == classNames.end()) throw spif_ref_error("Unknown classification in exclusion");
			t->excluded(*(*it).second);
		}
	}

	std::shared_ptr<Classification> parseClassification(rapidxml::xml_node<>::ptr classification, std::map<std::string,std::string> const & equivPolicies) {
		auto lacv_a = classification->first_attribute("lacv");
		std::integral auto lacv = Internal::str2num<lacv_t>(lacv_a->value());
		auto name_a = classification->first_attribute("name");
		std::string name{name_a->value()};
		auto hierarchy_a = classification->first_attribute("hierarchy");
		std::integral auto hierarchy = Internal::str2num<unsigned long>(hierarchy_a->value());
		std::shared_ptr<Classification> cls = std::make_shared<Classification>(lacv, name, hierarchy);
		for (auto reqCat = classification->first_node("requiredCategory"); reqCat; reqCat = reqCat->next_sibling("requiredCategory")) {
			cls->addRequiredCategory(parseCategoryGroup(reqCat));
		}
		for (auto equivClass = classification->first_node("equivalentClassification"); equivClass; equivClass = equivClass->next_sibling("equivalentClassification")) {
			auto equivName = equivClass->first_attribute("policyRef");
			if (!equivName || equivName->value().empty()) throw spif_syntax_error("Equivalent Classification requires policyRef");
            std::string equivNameStr{equivName->value()};
			auto i = equivPolicies.find(equivNameStr);
			if (i == equivPolicies.end()) throw spif_ref_error("PolicyRef not found");
			auto llacv_a = equivClass->first_attribute("lacv");
			std::integral auto llacv = Internal::str2num<lacv_t>(llacv_a->value());
			auto when_a = equivClass->first_attribute("applied");
			if (!when_a || when_a->value().empty()) throw spif_syntax_error("Missing applied in equivClass");
			std::shared_ptr<EquivClassification> equiv = std::make_shared<EquivClassification>((*i).second, llacv);
			for (auto reqCat = equivClass->first_node("requiredCategory"); reqCat; reqCat = reqCat->next_sibling("requiredCategory")) {
				equiv->addRequiredCategory(parseCategoryGroup(reqCat));
			}
			if (when_a->value() == "encrypt" || when_a->value() == "both") {
				cls->equivEncrypt(equiv);
			}
			if (when_a->value() == "decrypt" || when_a->value() == "both") {
				cls->equivDecrypt(equiv);
			}
		}
		cls->markings(parseMarkings(classification));
		auto color_a = classification->first_attribute("color");
		if (color_a && color_a->value().empty()) {
			cls->fgcolour(color_a->value());
		}
		return cls;
	}

	std::shared_ptr<EquivCat> parseEquivCat(rapidxml::xml_node<>::ptr equiv, std::map<std::string,std::string> const & policies) {
		auto policyref_a = equiv->first_attribute("policyRef");
		if (!policyref_a || policyref_a->value().empty()) throw spif_syntax_error("Missing PolicyRef");
        std::string policyref_s{policyref_a->value()};
		auto i = policies.find(policyref_s);
		if (i == policies.end()) throw spif_ref_error("PolicyRef not found");
		std::string policy_id = (*i).second;
		auto tagsetid_a = equiv->first_attribute("tagSetId");
		if (!tagsetid_a || tagsetid_a->value().empty()) throw spif_syntax_error("Missing tagSetId");
		TagType tagType = parseTagType(equiv);
		Lacv lacv = parseLacv(equiv->first_attribute("lacv"));
		bool discard = (equiv->first_attribute("action") != nullptr);
		if (discard) {
			return std::make_shared<EquivCat>(policy_id);
		} else {
			return std::make_shared<EquivCat>(policy_id, tagsetid_a->value(), tagType, lacv);
		}
	}

	std::shared_ptr<TagSet> parseTagSet(rapidxml::xml_node<>::ptr tagSet,
										size_t & ordinal,
										std::map<std::string, std::shared_ptr<Classification>> const & classNames,
										std::map<std::string,std::string> const & policies) {
		auto id_a = tagSet->first_attribute("id");
		auto name_a = tagSet->first_attribute("name");
		std::shared_ptr<TagSet> ts = std::make_shared<TagSet>(id_a->value(), name_a->value());
		ts->markings(parseMarkings(tagSet));
		for (auto tag = tagSet->first_node("securityCategoryTag"); tag; tag = tag->next_sibling("securityCategoryTag")) {
			auto tagName_a = tag->first_attribute("name");
			if (!tagName_a) throw spif_syntax_error("securityCategoryTag has no name");
			TagType tagType = parseTagType(tag);
			InformativeEncoding t7enc = InformativeEncoding::notApplicable;
			if (tagType == TagType::informative) {
				auto t7type_a = tag->first_attribute("tag7Encoding");
				if (!t7type_a) throw spif_syntax_error("element has no tag7Encoding");
				if (t7type_a->value() == "bitSetAttributes") {
					t7enc = InformativeEncoding::bitSet;
				} else {
					t7enc = InformativeEncoding::enumerated;
				}
			}
			std::shared_ptr<Tag> t = std::make_shared<Tag>(*ts, tagType, t7enc, tagName_a->value());
			ts->addTag(t);
			t->markings(parseMarkings(tag));
			for (auto cat = tag->first_node("tagCategory"); cat; cat = cat->next_sibling("tagCategory")) {
				Lacv lacv = parseLacv(cat->first_attribute("lacv"));
				std::shared_ptr<Category> c = std::make_shared<Category>(*t, cat->first_attribute("name")->value(), lacv, ordinal++);
				parseExcludedClass(cat, c, classNames);
				t->addCategory(c);
				for (auto reqnode = cat->first_node("requiredCategory"); reqnode; reqnode = reqnode->next_sibling("requiredCategory")) {
					std::unique_ptr<CategoryGroup> req = parseCategoryGroup(reqnode);
					c->required(std::move(req));
				}
				for (auto excnode = cat->first_node("excludedCategory"); excnode; excnode = excnode->next_sibling("excludedCategory")) {
					std::unique_ptr<CategoryData> exc = parseOptionalCategoryData(excnode);
					c->excluded(std::move(exc));
				}
				for (auto equiv = cat->first_node("equivalentSecCategoryTag"); equiv; equiv = equiv->next_sibling("equivalentSecCategoryTag")) {
					std::shared_ptr<EquivCat> ec = parseEquivCat(equiv, policies);
					c->encryptEquiv(ec);
				}
				c->markings(parseMarkings(cat));
			}
		}
		return ts;
	}
}

void Spif::parse(std::string const & s, Format fmt) {
	using namespace rapidxml;
	if (s.empty()) {
		throw spif_error("SPIF is empty");
	}
	std::string scratch = s;
	xml_document<> doc;
	doc.parse<0>(const_cast<char *>(scratch.c_str()));
	auto node = doc.first_node();
	if (!node) {
		throw spif_error("SPIF XML parse failure, no root node");
	}
	if (std::string("SPIF") != node->name()) {
		throw spif_error("Not a spif");
	}
	auto rbacId = node->first_attribute("rbacId");
	if (rbacId && !rbacId->value().empty()) {
		m_rbacId = rbacId->value();
	} else {
		m_rbacId = OID::NATO;
	}
	auto privilegeId = node->first_attribute("privilegeId");
	if (privilegeId && !privilegeId->value().empty()) {
		m_privilegeId = privilegeId->value();
	} else {
		m_privilegeId = OID::NATO;
	}
	auto securityPolicyId = node->first_node("securityPolicyId");
	if (securityPolicyId) {
		auto name = securityPolicyId->first_attribute("name");
		if (name) {
			m_name = name->value();
		}
		auto id = securityPolicyId->first_attribute("id");
		if (id) {
			m_oid = id->value();
		}
	}
	auto equivPolicies = node->first_node("equivalentPolicies");
	if (equivPolicies) {
		for (auto equivPolicy = equivPolicies->first_node(
				"equivalentPolicy"); equivPolicy; equivPolicy = equivPolicy->next_sibling("equivalentPolicy")) {
			auto name_a = equivPolicy->first_attribute("name");
			if (!name_a || name_a->value().empty()) throw spif_syntax_error("Equivalent policy requires name");
			std::string name{name_a->value()};
			auto id_a = equivPolicy->first_attribute("id");
			if (!id_a || id_a->value().empty()) throw spif_syntax_error("Equivalent policy requires id");
			std::string id{id_a->value()};
			m_equivPolicies.insert(std::make_pair(name, id));
			for (auto reqCat = equivPolicy->first_node("requiredCategory"); reqCat; reqCat = reqCat->next_sibling(
					"requiredCategory")) {
				m_equivReqs.insert(std::make_pair(id, parseCategoryGroup(reqCat)));
			}
		}
	}
	auto securityClassifications = node->first_node("securityClassifications");
	for (auto classn = securityClassifications->first_node("securityClassification"); classn; classn = classn->next_sibling("securityClassification")) {
		std::shared_ptr<Classification> c = parseClassification(classn, m_equivPolicies);
		auto ins = m_classifications.insert(std::make_pair(c->lacv(), c));
		if (!ins.second) {
			throw spif_invariant_error("Duplicate classification " + c->name());
		}
		auto ins2 = m_classnames.insert(std::make_pair(c->name(), c));
		if (!ins2.second) {
			throw spif_invariant_error("Duplicate classification name " + c->name());
		}
	}
	auto securityCategoryTagSets = node->first_node("securityCategoryTagSets");
	if (securityCategoryTagSets) {
		size_t ordinal = 0;
		for (auto tagSet = securityCategoryTagSets->first_node("securityCategoryTagSet"); tagSet; tagSet = tagSet->next_sibling("securityCategoryTagSet")) {
			std::shared_ptr<TagSet> ts = parseTagSet(tagSet, ordinal, m_classnames, m_equivPolicies);
			bool inserted;
			std::tie(std::ignore, inserted) = m_tagSets.insert(std::make_pair(ts->id(), ts));
			if (!inserted) throw spif_invariant_error("Duplicate TagSet id " + ts->id());
			std::tie(std::ignore, inserted) = m_tagSetsByName.insert(std::make_pair(ts->name(), ts));
			if (!inserted) throw spif_invariant_error("Duplicate TagSet name " + ts->id());
		}
	}
	m_markings = parseMarkings(node);
	// Once we've parsed all the tagSets, we need to "compile" the category restrictions.
	for (auto & ts : m_tagSets) {
		ts.second->compile(*this);
	}
	for (auto & cls : m_classifications) {
		cls.second->compile(*this);
	}
}

namespace {
	void categoryMarkings(MarkingCode loc, std::string const & langTag, std::string & marking, std::set<CategoryRef> const & cats, std::string const & sep) {
		std::string tagName;
		std::string tagsep;
		std::string tagSuffix;
		for (auto & i : cats) {
			std::string phrase;
			Marking const * markingData = nullptr;
			if (i->hasMarkings()) {
				markingData = i->markings().marking(langTag);
			}
			if (!markingData && i->tag().hasMarkings()) {
				markingData = i->tag().markings().marking(langTag);
			}
			if (markingData) {
				phrase = markingData->phrase(loc, i->name());
			} else {
				phrase = i->name();
			}
			if (phrase.empty()) continue;
			std::string currentTagName = i->tag().name();
			if (tagName != currentTagName) {
				// Entering new tag.
				tagName = currentTagName;
				marking += tagSuffix;
				if (!marking.empty()) marking += sep;
				markingData = nullptr;
				if (i->tag().hasMarkings()) {
					markingData = i->tag().markings().marking(langTag);
				}
				if (markingData) {
					tagSuffix = markingData->suffix();
					tagsep = markingData->sep();
					if (tagsep.empty()) tagsep = "/"; // Default;
					marking += markingData->prefix();
				} else {
					tagSuffix = "";
					tagsep = "/";
				}
			} else {
				marking += tagsep;
			}
			marking += phrase;
		}
		marking += tagSuffix;
	}
}

std::string Spif::displayMarking(Label const & label, std::string const & langTag, MarkingCode loc) const {
	std::string marking;
	if (label.policy_id() != m_oid) {
		throw policy_mismatch("Label is incorrect policy");
	}
	bool suppressClassName = false;
	for (auto & i : label.categories()) {
		if (i->hasMarkings()) {
			Marking const * m;
			if ((m = i->markings().marking(langTag)) && m->suppressClassName(loc)) {
				suppressClassName = true;
				break;
			}
		}
	}
	std::string sep;
	Marking const * markingData = nullptr;
	if (m_markings != nullptr) markingData = m_markings->marking(langTag);
	if (markingData != nullptr) sep = markingData->sep();
	if (sep.empty()) sep = " "; // Default.
	// Print policy name.
	Marking const * clsMarking = nullptr;
	if (label.classification().hasMarkings()) clsMarking = label.classification().markings().marking(langTag);
	if (markingData && markingData->replacePolicy(loc)) {
		marking += markingData->policyPhrase(loc, m_name);
	} else if (clsMarking && clsMarking->replacePolicy(loc)) {
		marking += clsMarking->policyPhrase(loc, label.classification().name());
	} else {
		bool replacedPolicy = false;
		for (auto & i : label.categories()) {
			if (i->hasMarkings()) {
				Marking const * m;
				if ((m = i->markings().marking(langTag)) && m->replacePolicy(loc)) {
					marking += m->policyPhrase(loc, i->name());
					replacedPolicy = true;
					break;
				}
			}
		}
		if (!replacedPolicy) {
			marking += m_name;
		}
	}
	if (!suppressClassName) {
		if (!marking.empty()) marking += sep;
		if (clsMarking) {
			marking += clsMarking->phrase(loc, label.classification().name());
		} else if (markingData != nullptr) {
			marking += markingData->phrase(loc, label.classification().name());
		} else {
			marking += label.classification().name();
		}
	}
	categoryMarkings(loc, langTag, marking, label.categories(), sep);
	if (markingData != nullptr) marking = markingData->prefix() + marking;
	if (markingData != nullptr) marking += markingData->suffix();
	return marking;
}

std::string Spif::displayMarking(Spiffing::Clearance const & clearance, std::string const & langTag) const {
	std::string marking;
	bool any{false};
	if (clearance.policy_id() != m_oid) {
		throw policy_mismatch("Clearance is incorrect policy");
	}
	std::string sep;
	Marking const * markingData = nullptr;
	if (m_markings != nullptr) markingData = m_markings->marking(langTag);
	if (markingData != nullptr) marking += markingData->prefix();
	if (markingData != nullptr) sep = markingData->sep();
	if (sep.empty()) sep = " "; // Default.
	marking += "{";
	std::set<std::shared_ptr<Classification>, ClassificationHierarchyCompare> classes;
	for (auto cls : clearance.classifications()) {
		auto clsit = m_classifications.find(cls);
		if (clsit == m_classifications.end()) {
			throw clearance_error("No classification (from clearance)");
		}
		classes.insert((*clsit).second);
	}
	for (auto const & cls : classes) {
		if (!any) {
			any = true;
		} else {
			marking += "|";
		}
		marking += cls->name();
	}
	if (!any) {
		auto clsit = m_classifications.find(0);
		if (clsit == m_classifications.end()) {
			marking += "unclassified (auto)";
		} else {
			marking += (*clsit).second->name();
		}
	}
	marking += "}";
	categoryMarkings(MarkingCode::pageBottom, langTag, marking, clearance.categories(), sep);
	if (markingData != nullptr) marking += markingData->suffix();
	return marking;
}

bool Spif::acdf(Label const & label, Spiffing::Clearance const & clearance) const {
	if (clearance.policy_id() != m_oid) {
		throw policy_mismatch("Clearance is incorrect policy");
	}
	if (label.policy_id() != m_oid) {
		throw policy_mismatch("Label is incorrect policy");
	}
	if (!clearance.hasClassification(label.classification().lacv())) return false;
	// Now consider each category in the label.
	std::string permissiveTagName;
	std::set<CategoryRef> cats;
	for (auto & cat : label.categories()) {
		Tag const & tag = cat->tag();
		if (!permissiveTagName.empty() && permissiveTagName != tag.name()) {
			if (!clearance.hasCategory(cats)) return false;
			permissiveTagName.clear();
			cats.clear();
		}
		if (tag.tagType() == TagType::restrictive ||
				tag.tagType() == TagType::enumeratedRestrictive) {
				if (!clearance.hasCategory(cat)) return false;
		} else if (tag.tagType() == TagType::permissive ||
								tag.tagType() == TagType::enumeratedPermissive) {
				cats.insert(cat);
				permissiveTagName = tag.name();
		}
	}
	if (!permissiveTagName.empty()) {
		if (!clearance.hasCategory(cats)) return false;
		permissiveTagName.clear();
		cats.clear();
	}
	return true;
}

std::shared_ptr<TagSet> const & Spif::tagSetLookup(std::string_view const & tagSetIdv) const {
    std::string tagSetId{tagSetIdv};
	auto i = m_tagSets.find(tagSetId);
	if (i == m_tagSets.end()) throw spif_ref_error("Unknown tagset id: " + tagSetId);
	return (*i).second;
}

std::shared_ptr<TagSet> const & Spif::tagSetLookupByName(std::string_view const & tagSetv) const {
    std::string tagSet{tagSetv};
	auto i = m_tagSetsByName.find(tagSet);
	if (i == m_tagSetsByName.end()) throw spif_ref_error("Unknown tagset name: " + tagSet);
	return (*i).second;
}

bool Spif::valid(Label const & label) const {
	if (!label.classification().valid(label)) return false;
	for (auto & cat : label.categories()) {
		if (!cat->valid(label)) return false;
	}
	return true;
}

void Spif::encrypt(Label & label) const {
	auto end = m_equivReqs.upper_bound(label.policy_id());
	for (auto i = m_equivReqs.lower_bound(label.policy_id()); i != end; ++i) {
		(*i).second->fixup(label);
	}
}
