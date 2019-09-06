// Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FORTRAN_PARSER_PROVENANCE_H_
#define FORTRAN_PARSER_PROVENANCE_H_

#include "char-block.h"
#include "char-buffer.h"
#include "characters.h"
#include "source.h"
#include "../common/idioms.h"
#include "../common/interval.h"
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Fortran::parser {

// Each character in the contiguous source stream built by the
// prescanner corresponds to a particular character in a source file,
// include file, macro expansion, or compiler-inserted padding.
// The location of this original character to which a parsable character
// corresponds is its provenance.
//
// Provenances are offsets into an (unmaterialized) marshaling of the
// entire contents of all the original source files, include files, macro
// expansions, &c. for each visit to each source.  These origins of the
// original source characters constitute a forest whose roots are
// the original source files named on the compiler's command line.
// Given a Provenance, we can find the tree node that contains it in time
// O(log(# of origins)), and describe the position precisely by walking
// up the tree.  (It would be possible, via a time/space trade-off, to
// cap the time by the use of an intermediate table that would be indexed
// by the upper bits of an offset, but that does not appear to be
// necessary.)

class AllSources;

class Provenance {
public:
  Provenance() {}
  Provenance(std::size_t offset) : offset_{offset} { CHECK(offset > 0); }
  Provenance(const Provenance &that) = default;
  Provenance(Provenance &&that) = default;
  Provenance &operator=(const Provenance &that) = default;
  Provenance &operator=(Provenance &&that) = default;

  std::size_t offset() const { return offset_; }

  Provenance operator+(ptrdiff_t n) const {
    CHECK(n > -static_cast<ptrdiff_t>(offset_));
    return {offset_ + static_cast<std::size_t>(n)};
  }
  Provenance operator+(std::size_t n) const { return {offset_ + n}; }
  std::size_t operator-(Provenance that) const {
    CHECK(that <= *this);
    return offset_ - that.offset_;
  }
  bool operator<(Provenance that) const { return offset_ < that.offset_; }
  bool operator<=(Provenance that) const { return !(that < *this); }
  bool operator==(Provenance that) const { return offset_ == that.offset_; }
  bool operator!=(Provenance that) const { return !(*this == that); }

private:
  std::size_t offset_{0};
};

using ProvenanceRange = common::Interval<Provenance>;

// Maps contiguous ranges of byte offsets in original source files to
// contiguous ranges in the cooked character stream; essentially a
// partial inversion of OffsetToProvenanceMappings (below).
// Used for implementing the first step of mapping an identifier
// selected in a code editor to one of its declarative statements.
class ProvenanceRangeToOffsetMappings {
public:
  ProvenanceRangeToOffsetMappings();
  ~ProvenanceRangeToOffsetMappings();
  bool empty() const { return map_.empty(); }
  void Put(ProvenanceRange, std::size_t offset);
  std::optional<std::size_t> Map(ProvenanceRange) const;
  std::ostream &Dump(std::ostream &) const;

private:
  // A comparison function object for use in std::multimap<Compare=>.
  // Intersecting intervals will effectively compare equal, not being
  // either < nor >= each other.
  struct WhollyPrecedes {
    bool operator()(ProvenanceRange, ProvenanceRange) const;
  };

  std::multimap<ProvenanceRange, std::size_t, WhollyPrecedes> map_;
};

// Maps 0-based local offsets in some contiguous range (e.g., a token
// sequence) to their provenances.  Lookup time is on the order of
// O(log(#of intervals with contiguous provenances)).  As mentioned
// above, this time could be capped via a time/space trade-off.
class OffsetToProvenanceMappings {
public:
  OffsetToProvenanceMappings() {}
  void clear();
  void swap(OffsetToProvenanceMappings &);
  void shrink_to_fit();
  std::size_t SizeInBytes() const;
  void Put(ProvenanceRange);
  void Put(const OffsetToProvenanceMappings &);
  ProvenanceRange Map(std::size_t at) const;
  void RemoveLastBytes(std::size_t);
  ProvenanceRangeToOffsetMappings Invert(const AllSources &) const;
  std::ostream &Dump(std::ostream &) const;

private:
  struct ContiguousProvenanceMapping {
    std::size_t start;
    ProvenanceRange range;
  };

  // Elements appear in ascending order of distinct .start values;
  // their .range values are disjoint and not necessarily adjacent.
  std::vector<ContiguousProvenanceMapping> provenanceMap_;
};

// A singleton AllSources instance for the whole compilation
// is shared by reference.
class AllSources {
public:
  AllSources();
  ~AllSources();

  std::size_t size() const { return range_.size(); }
  const char &operator[](Provenance) const;
  Encoding encoding() const { return encoding_; }
  AllSources &set_encoding(Encoding e) {
    encoding_ = e;
    return *this;
  }

  void PushSearchPathDirectory(std::string);
  std::string PopSearchPathDirectory();
  const SourceFile *Open(std::string path, std::stringstream *error);
  const SourceFile *ReadStandardInput(std::stringstream *error);

  ProvenanceRange AddIncludedFile(
      const SourceFile &, ProvenanceRange, bool isModule = false);
  ProvenanceRange AddMacroCall(
      ProvenanceRange def, ProvenanceRange use, const std::string &expansion);
  ProvenanceRange AddCompilerInsertion(std::string);

  bool IsValid(Provenance at) const { return range_.Contains(at); }
  bool IsValid(ProvenanceRange range) const {
    return range.size() > 0 && range_.Contains(range);
  }
  void EmitMessage(std::ostream &, const std::optional<ProvenanceRange> &,
      const std::string &message, bool echoSourceLine = false) const;
  const SourceFile *GetSourceFile(
      Provenance, std::size_t *offset = nullptr) const;
  std::string GetPath(Provenance) const;  // __FILE__
  int GetLineNumber(Provenance) const;  // __LINE__
  Provenance CompilerInsertionProvenance(char ch);
  Provenance CompilerInsertionProvenance(const char *, std::size_t);
  ProvenanceRange IntersectionWithSourceFiles(ProvenanceRange) const;
  std::ostream &Dump(std::ostream &) const;

private:
  struct Inclusion {
    const SourceFile &source;
    bool isModule{false};
  };
  struct Macro {
    ProvenanceRange definition;
    std::string expansion;
  };
  struct CompilerInsertion {
    std::string text;
  };

  struct Origin {
    Origin(ProvenanceRange, const SourceFile &);
    Origin(ProvenanceRange, const SourceFile &, ProvenanceRange,
        bool isModule = false);
    Origin(ProvenanceRange, ProvenanceRange def, ProvenanceRange use,
        const std::string &expansion);
    Origin(ProvenanceRange, const std::string &);

    const char &operator[](std::size_t) const;

    std::variant<Inclusion, Macro, CompilerInsertion> u;
    ProvenanceRange covers, replaces;
  };

  const Origin &MapToOrigin(Provenance) const;

  // Elements are in ascending & contiguous order of .covers.
  std::vector<Origin> origin_;
  ProvenanceRange range_;
  std::map<char, Provenance> compilerInsertionProvenance_;
  std::vector<std::unique_ptr<SourceFile>> ownedSourceFiles_;
  std::vector<std::string> searchPath_;
  Encoding encoding_{Encoding::UTF_8};
};

class CookedSource {
public:
  explicit CookedSource(AllSources &);
  ~CookedSource();

  AllSources &allSources() { return allSources_; }
  const AllSources &allSources() const { return allSources_; }
  const std::string &data() const { return data_; }

  bool IsValid(const char *p) const {
    return p >= &data_.front() && p <= &data_.back() + 1;
  }
  bool IsValid(CharBlock range) const {
    return !range.empty() && IsValid(range.begin()) && IsValid(range.end() - 1);
  }
  bool IsValid(ProvenanceRange r) const { return allSources_.IsValid(r); }

  std::optional<ProvenanceRange> GetProvenanceRange(CharBlock) const;
  std::optional<CharBlock> GetCharBlock(ProvenanceRange) const;

  // The result of a Put() is the offset that the new data
  // will have in the eventually marshaled contiguous buffer.
  std::size_t Put(const char *data, std::size_t bytes) {
    return buffer_.Put(data, bytes);
  }
  std::size_t Put(const std::string &s) { return buffer_.Put(s); }
  std::size_t Put(char ch) { return buffer_.Put(&ch, 1); }
  std::size_t Put(char ch, Provenance p) {
    provenanceMap_.Put(ProvenanceRange{p, 1});
    return buffer_.Put(&ch, 1);
  }

  void PutProvenance(Provenance p) { provenanceMap_.Put(ProvenanceRange{p}); }
  void PutProvenance(ProvenanceRange pr) { provenanceMap_.Put(pr); }
  void PutProvenanceMappings(const OffsetToProvenanceMappings &pm) {
    provenanceMap_.Put(pm);
  }

  void Marshal();  // marshals text into one contiguous block
  void CompileProvenanceRangeToOffsetMappings();
  std::string AcquireData() { return std::move(data_); }
  std::ostream &Dump(std::ostream &) const;

private:
  AllSources &allSources_;
  CharBuffer buffer_;  // before Marshal()
  std::string data_;  // all of it, prescanned and preprocessed
  OffsetToProvenanceMappings provenanceMap_;
  ProvenanceRangeToOffsetMappings invertedMap_;
};
}
#endif  // FORTRAN_PARSER_PROVENANCE_H_
