/***

Copyright 2014 Dave Cridland
Copyright 2014 Surevine Ltd

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
#include <spiffing/label.h>
#include <spiffing/clearance.h>
#include <spiffing/spiffing.h>
#include <fstream>
#include <iostream>
#include <rapidxml.hpp>

bool test_step(std::size_t testno, rapidxml::xml_node<>::ptr test) {
	using namespace Spiffing;
	try {
		std::ifstream label_file(std::string{test->first_attribute("label")->value()});
		std::string label_str{std::istreambuf_iterator<char>(label_file), std::istreambuf_iterator<char>()};
		std::unique_ptr<Label> label(new Label(label_str, Format::ANY));
		std::string langTag;
		if (test->first_attribute("valid")) {
			bool result = label->policy().valid(*label);
			bool expected = (test->first_attribute("valid")->value()[0] == 't');
			if (result != expected) throw std::runtime_error(std::string("Validity failure: Expected ") + (expected ? "VALID" : "NOT valid"));
		}
		if (test->first_attribute("xml:lang")) {
			langTag = test->first_attribute("xml:lang")->value();
		}
		if (test->first_attribute("label-marking")) {
			std::string label_marking = label->policy().displayMarking(*label, langTag);
			if (label_marking != test->first_attribute("label-marking")->value()) throw std::runtime_error("Mismatching label marking: " + label_marking);
		}
		if (test->first_attribute("encrypt")) {
			label = label->encrypt(test->first_attribute("encrypt")->value());
		}
		if (test->first_attribute("encrypt-marking")) {
			std::string label_marking = label->policy().displayMarking(*label, langTag);
			if (label_marking != test->first_attribute("encrypt-marking")->value()) throw std::runtime_error("Mismatching encrypt-label marking: " + label_marking);
		}
		if (test->first_attribute("clearance")) {
			std::ifstream clearance_file(std::string{test->first_attribute("clearance")->value()});
			std::string clearance_str{std::istreambuf_iterator<char>(clearance_file), std::istreambuf_iterator<char>()};
			Clearance clearance(clearance_str, Format::ANY);
			if(test->first_attribute("clearance-marking")) {
				std::string clearance_marking = clearance.policy().displayMarking(clearance);
				if (clearance_marking != test->first_attribute("clearance-marking")->value()) throw std::runtime_error("Mismatching clearance marking: " + clearance_marking);
			}
			bool acdf = (test->first_attribute("acdf-result")->value()[0] == '1');
			if (acdf != clearance.policy().acdf(*label, clearance)) throw std::runtime_error(std::string("ACDF result is unexpected ") + (acdf ? "FAIL" : "PASS"));
		}
		auto ex = test->first_attribute("exception");
		if (ex) {
			std::cout << "Test " << testno << " expected exception: " << ex->value() << std::endl;
			return false;
		}
		return true;
	} catch (std::exception & e) {
		auto ex = test->first_attribute("exception");
		if (ex && std::string(ex->value()) == e.what()) return true; // Expected this error; pass.
		std::cout << "Test " << testno << " failed: " << e.what() << std::endl;
	}
	return false;
}

int main(int argc, char *argv[]) {
	using namespace rapidxml;
	try {
		if (argc <= 1) {
			std::cout << "test tests.xml" << std::endl;
			return 0;
		}
		std::ifstream file(argv[1]);
		std::string str{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
		xml_document<> doc;
		doc.parse<0>(const_cast<char *>(str.c_str()));
		auto root = doc.first_node();
		std::size_t testno = 0;
		std::size_t passes = 0;
		std::cout << "Tests initiaterizing." << std::endl;
		for (auto seq = root->first_node("sequence"); seq; seq = seq->next_sibling("sequence")) {
			Spiffing::Site site;
			for (auto pol = seq->first_node("policy"); pol; pol = pol->next_sibling("policy")) {
				try {
					++testno;
					auto spif = pol->first_attribute("spif");
					std::ifstream spiffile{std::string{spif->value()}};
					site.load(spiffile);
					auto ex = pol->first_attribute("exception");
					if (ex) throw std::runtime_error("Expected exception " + std::string(ex->value()));
					++passes;
				} catch(std::exception &e) {
					auto ex = pol->first_attribute("exception");
					if (ex && std::string(ex->value()) == e.what()) {
						++passes;
						continue;
					} // Expected this error; pass.
					std::cout << "Policy load at " << testno << " failed: " << e.what() << std::endl;
				}
			}
			for (auto test = seq->first_node("test"); test; test = test->next_sibling("test")) {
				++testno;
				if (test_step(testno, test)) {
					++passes;
				}
			}
		}
		std::cout << "[" << passes << "/" << testno << "] Tests passed." << std::endl;
		if (passes != testno) return 2;
	} catch(std::exception & e) {
		std::cerr << "Dude, exception: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
