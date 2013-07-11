// Copyright (c) 2013, Cloudera, inc.
#ifndef KUDU_TABLET_TABLET_TEST_UTIL_H
#define KUDU_TABLET_TABLET_TEST_UTIL_H

#include <string>
#include <vector>

#include "common/iterator.h"
#include "gutil/strings/join.h"
#include "tablet/tablet.h"
#include "util/test_util.h"

namespace kudu {
namespace tablet {

using std::string;
using std::vector;

class KuduTabletTest : public KuduTest {
 public:
  KuduTabletTest(const Schema& schema)
    : schema_(schema)
  {}

  virtual void SetUp() {
    KuduTest::SetUp();

    SetUpTestTablet();
    ASSERT_STATUS_OK(tablet_->CreateNew());
  }

  void SetUpTestTablet(const string& root_dir = "") {
    metadata::TabletMasterBlockPB master_block;
    master_block.set_block_a("00000000000000000000000000000000");
    master_block.set_block_b("11111111111111111111111111111111");

    fs_manager_.reset(new FsManager(env_.get(), root_dir.empty() ? test_dir_ : root_dir));
    gscoped_ptr<metadata::TabletMetadata> metadata_(
      new metadata::TabletMetadata(fs_manager_.get(), "KuduTabletTestId", master_block));
    tablet_.reset(new Tablet(metadata_.Pass(), schema_));
  }

  void TabletReOpen(const string& root_dir = "") {
    SetUpTestTablet(root_dir);
    ASSERT_STATUS_OK(tablet_->Open());
  }

  const Schema &schema() const {
    return schema_;
  }

 protected:
  const Schema schema_;
  gscoped_ptr<Tablet> tablet_;
  gscoped_ptr<FsManager> fs_manager_;
};

class KuduRowSetTest : public KuduTabletTest {
 public:
  KuduRowSetTest(const Schema& schema)
    : KuduTabletTest(schema)
  {}

  virtual void SetUp() {
    KuduTabletTest::SetUp();
    ASSERT_STATUS_OK(NewRowSet(&rowset_meta_));
  }

  Status NewRowSet(shared_ptr<metadata::RowSetMetadata> *rowset_meta) {
    return const_cast<metadata::TabletMetadata *>(tablet_->metadata())->CreateRowSet(rowset_meta);
  }

  Status FlushMetadata() {
    return const_cast<metadata::TabletMetadata *>(tablet_->metadata())->Flush();
  }

 protected:
  shared_ptr<metadata::RowSetMetadata> rowset_meta_;
};

static inline Status IterateToStringList(RowwiseIterator *iter,
                                         vector<string> *out,
                                         int limit = INT_MAX) {
  out->clear();
  Schema schema = iter->schema();
  Arena arena(1024, 1024);
  RowBlock block(schema, 100, &arena);
  int fetched = 0;
  while (iter->HasNext() && fetched < limit) {
    RETURN_NOT_OK(RowwiseIterator::CopyBlock(iter, &block));
    for (size_t i = 0; i < block.nrows() && fetched < limit; i++) {
      if (block.selection_vector()->IsRowSelected(i)) {
        out->push_back(schema.DebugRow(block.row(i)));
        fetched++;
      }
    }
  }
  return Status::OK();
}

// Construct a new iterator from the given rowset, and dump
// all of its results into 'out'. The previous contents
// of 'out' are cleared.
static inline Status DumpRowSet(const RowSet &rs,
                                const Schema &projection,
                                const MvccSnapshot &snap,
                                vector<string> *out,
                                int limit = INT_MAX) {
  gscoped_ptr<RowwiseIterator> iter(rs.NewRowIterator(projection, snap));
  RETURN_NOT_OK(iter->Init(NULL));
  RETURN_NOT_OK(IterateToStringList(iter.get(), out, limit));
  return Status::OK();
}

// Take an un-initialized iterator, Init() it, and iterate through all of its rows.
// The resulting string contains a line per entry.
static inline string InitAndDumpIterator(gscoped_ptr<RowwiseIterator> iter) {
  CHECK_OK(iter->Init(NULL));

  vector<string> out;
  CHECK_OK(IterateToStringList(iter.get(), &out));
  return JoinStrings(out, "\n");

}

} // namespace tablet
} // namespace kudu
#endif
