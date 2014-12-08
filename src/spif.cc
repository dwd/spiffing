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

using namespace Spiffing;

Spif::Spif()
: m_classifications() {
}

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
	std::shared_ptr<Classification> parseClassification(rapidxml::xml_node<> * classification) {
		auto lacv_a = classification->first_attribute("lacv");
		lacv_t lacv{strtoull(lacv_a->value(), NULL, 10)};
		auto name_a = classification->first_attribute("name");
		std::string name{name_a->value()};
		auto hierarchy_a = classification->first_attribute("hierarchy");
		unsigned long hierarchy{strtoul(hierarchy_a->value(), NULL, 10)};
		std::shared_ptr<Classification> cls{new Classification(lacv, name, hierarchy)};
		return cls;
	}

	Lacv parseLacv(rapidxml::xml_attribute<> * lacv) {
		return Lacv::parse(std::string{(char *)lacv->value(), lacv->value_size()});
	}

	std::unique_ptr<Marking> parseMarking(rapidxml::xml_node<> * holder) {
		std::unique_ptr<Marking> ptr;
		for (auto qual = holder->first_node("markingQualifier"); qual; qual = qual->next_sibling("markingQualifier")) {
			for (auto q = qual->first_node("qualifier"); q; q = q->next_sibling("qualifier")) {
				auto qcode_a = q->first_attribute("qualifierCode");
				auto txt_a = q->first_attribute("markingQualifier");
				if (!txt_a) continue;
				std::string txt(txt_a->value(), txt_a->value_size());
				if (qcode_a) {
					std::string qcode(qcode_a->value(), qcode_a->value_size());
					if (!ptr) ptr = std::unique_ptr<Marking>{new Marking()};
					if (qcode == "prefix") {
						ptr->prefix(txt);
					} else if (qcode == "suffix") {
						ptr->suffix(txt);
					} else if (qcode == "separator") {
						ptr->sep(txt);
					}
				}
			}
		}
		return ptr;
	}

	std::shared_ptr<TagSet> parseTagSet(rapidxml::xml_node<> * tagSet, size_t & ordinal) {
		auto id_a = tagSet->first_attribute("id");
		auto name_a = tagSet->first_attribute("name");
		std::shared_ptr<TagSet> ts{new TagSet(id_a->value(), name_a->value())};
		for (auto tag = tagSet->first_node("securityCategoryTag"); tag; tag = tag->next_sibling("securityCategoryTag")) {
			auto tagName_a = tag->first_attribute("name");
			if (!tagName_a) throw std::runtime_error("securityCategoryTag has no name");
			auto tagType_a = tag->first_attribute("tagType");
			if (!tagType_a) throw std::runtime_error("securityCategoryTag has no tagType");
			std::string tagTypeName = tagType_a->value();
			TagType tagType;
			if (tagTypeName == "permissive") {
				tagType = TagType::permissive;
			} else if (tagTypeName == "restrictive") {
				tagType = TagType::restrictive;
			} else if (tagTypeName == "enumerated") {
				auto enumType_a = tag->first_attribute("enumType");
				if (!enumType_a) throw std::runtime_error("securityCategoryTag has no enumType");
				std::string enumTypeName = enumType_a->value();
				if (enumTypeName == "permissive") {
					tagType = TagType::enumeratedPermissive;
				} else {
					tagType = TagType::enumeratedRestrictive;
				}
			} else {
				// TODO :: tagType7; ignored for now.
				continue;
			}
			std::shared_ptr<Tag> t{new Tag(*ts, tagType, tagName_a->value())};
			ts->addTag(t);
			t->marking(parseMarking(tag));
			for (auto cat = tag->first_node("tagCategory"); cat; cat = cat->next_sibling("tagCategory")) {
				Lacv lacv = parseLacv(cat->first_attribute("lacv"));
				std::shared_ptr<Category> c{new Category(*t, cat->first_attribute("name")->value(), lacv, ordinal++)};
				t->addCategory(c);
			}
		}
		return ts;
	}
}

void Spif::parse(std::string const & s, Format fmt) {
	using namespace rapidxml;
	std::string scratch = s;
	xml_document<> doc;
	doc.parse<0>(const_cast<char *>(scratch.c_str()));
	auto node = doc.first_node();
	if (std::string("SPIF") != node->name()) {
		throw std::runtime_error("Not a spif");
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
	auto securityClassifications = node->first_node("securityClassifications");
	for (auto classn = securityClassifications->first_node("securityClassification"); classn; classn = classn->next_sibling("securityClassification")) {
		std::shared_ptr<Classification> c = parseClassification(classn);
		auto ins = m_classifications.insert(std::make_pair(c->lacv(), c));
		if (!ins.second) {
			throw std::runtime_error("Duplicate classification ");
		}
	}
	auto securityCategoryTagSets = node->first_node("securityCategoryTagSets");
	if (securityCategoryTagSets) {
		size_t ordinal = 0;
		for (auto tagSet = securityCategoryTagSets->first_node("securityCategoryTagSet"); tagSet; tagSet = tagSet->next_sibling("securityCategoryTagSet")) {
			std::shared_ptr<TagSet> ts = parseTagSet(tagSet, ordinal);
			m_tagSets.insert(std::make_pair(ts->id(), ts));
		}
	}
	m_marking = parseMarking(node);
}

namespace {
	void categoryMarkings(std::string & marking, std::set<CategoryRef> const & cats, std::string const & sep) {
		std::string tagName;
		std::string tagsep;
		for (auto & i : cats) {
			std::string currentTagName = i->tag().name();
			if (tagName != currentTagName) {
				// Entering new tag.
				tagName = currentTagName;
				marking += sep;
				if (i->tag().hasMarking()) {
					tagsep = i->tag().marking().sep();
					if (tagsep.empty()) tagsep = " "; // Default;
					marking += i->tag().marking().prefix();
				} else {
					tagsep = " ";
				}
			} else {
				marking += tagsep;
			}
			marking += i->name();
		}
	}
}

std::string Spif::displayMarking(Label const & label) const {
	std::string marking;
	if (label.policy_id() != m_oid) {
		throw std::runtime_error("Label is incorrect policy");
	}
	if (m_marking != nullptr) marking = m_marking->prefix();
	std::string sep;
	if (m_marking != nullptr) sep = m_marking->sep();
	if (sep.empty()) sep = " "; // Default.
	marking += label.classification()->name();
	categoryMarkings(marking, label.categories(), sep);
	if (m_marking != nullptr) marking += m_marking->suffix();
	return marking;
}

std::string Spif::displayMarking(Spiffing::Clearance const & clearance) const {
	std::string marking{"{"};
	bool any{false};
	if (clearance.policy_id() != m_oid) {
		throw std::runtime_error("Clearance is incorrect policy");
	}
	std::string sep;
	if (m_marking != nullptr) sep = m_marking->sep();
	if (sep.empty()) sep = " "; // Default.
	for (auto cls : clearance.classifications()) {
		auto clsit = m_classifications.find(cls);
		if (clsit == m_classifications.end()) {
			throw std::runtime_error("No classification (from clearance)");
		}
		if (!any) {
			any = true;
		} else {
			marking += "|";
		}
		if (m_marking != nullptr) marking += m_marking->prefix();
		marking += (*clsit).second->name();
	}
	if (!any) {
		auto clsit = m_classifications.find(0);
		if (clsit == m_classifications.end()) {
			marking += "unclassified (auto)";
		} else {
			if (m_marking != nullptr) marking += m_marking->prefix();
			marking += (*clsit).second->name();
		}
	}
	marking += "}";
	categoryMarkings(marking, clearance.categories(), sep);
	if (m_marking != nullptr) marking += m_marking->suffix();
	return marking;
}

bool Spif::acdf(Label const & label, Spiffing::Clearance const & clearance) const {
	if (clearance.policy_id() != m_oid) {
		throw std::runtime_error("Clearance is incorrect policy");
	}
	if (label.policy_id() != m_oid) {
		throw std::runtime_error("Label is incorrect policy");
	}
	if (!clearance.hasClassification(label.classification()->lacv())) return false;
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

std::shared_ptr<TagSet> const & Spif::tagSetLookup(std::string const & tagSetId) const {
	auto i = m_tagSets.find(tagSetId);
	if (i == m_tagSets.end()) throw std::runtime_error("Unknown tagset id: " + tagSetId);
	return (*i).second;
}