// Copyright 2021-present StarRocks, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "storage/rowset/struct_column_iterator.h"

#include "column/nullable_column.h"
#include "column/struct_column.h"
#include "storage/rowset/scalar_column_iterator.h"

namespace starrocks {

class StructColumnIterator final : public ColumnIterator {
public:
    StructColumnIterator(std::unique_ptr<ColumnIterator> null_iter,
                         std::vector<std::unique_ptr<ColumnIterator>> field_iters);

    ~StructColumnIterator() override = default;

    Status init(const ColumnIteratorOptions& opts) override;

    Status next_batch(size_t* n, vectorized::Column* dst) override;

    Status next_batch(const vectorized::SparseRange& range, vectorized::Column* dst) override;

    Status seek_to_first() override;

    Status seek_to_ordinal(ordinal_t ord) override;

    ordinal_t get_current_ordinal() const override { return _field_iters[0]->get_current_ordinal(); }

    /// for vectorized engine
    Status get_row_ranges_by_zone_map(const std::vector<const vectorized::ColumnPredicate*>& predicates,
                                      const vectorized::ColumnPredicate* del_predicate,
                                      vectorized::SparseRange* row_ranges) override {
        CHECK(false) << "array column does not has zone map index";
        return Status::OK();
    }

    Status fetch_values_by_rowid(const rowid_t* rowids, size_t size, vectorized::Column* values) override;

private:
    std::unique_ptr<ColumnIterator> _null_iter;
    std::vector<std::unique_ptr<ColumnIterator>> _field_iters;
};

StatusOr<std::unique_ptr<ColumnIterator>> create_struct_iter(std::unique_ptr<ColumnIterator> null_iter,
                                                             std::vector<std::unique_ptr<ColumnIterator>> field_iters) {
    return std::make_unique<StructColumnIterator>(std::move(null_iter), std::move(field_iters));
}

StructColumnIterator::StructColumnIterator(std::unique_ptr<ColumnIterator> null_iter,
                                           std::vector<std::unique_ptr<ColumnIterator>> field_iters)
        : _null_iter(std::move(null_iter)), _field_iters(std::move(field_iters)) {}

Status StructColumnIterator::init(const ColumnIteratorOptions& opts) {
    if (_null_iter != nullptr) {
        RETURN_IF_ERROR(_null_iter->init(opts));
    }
    for (auto& iter : _field_iters) {
        RETURN_IF_ERROR(iter->init(opts));
    }
    return Status::OK();
}

Status StructColumnIterator::next_batch(size_t* n, vectorized::Column* dst) {
    vectorized::StructColumn* struct_column = nullptr;
    vectorized::NullColumn* null_column = nullptr;
    if (dst->is_nullable()) {
        auto* nullable_column = down_cast<vectorized::NullableColumn*>(dst);

        struct_column = down_cast<vectorized::StructColumn*>(nullable_column->data_column().get());
        null_column = down_cast<vectorized::NullColumn*>(nullable_column->null_column().get());
    } else {
        struct_column = down_cast<vectorized::StructColumn*>(dst);
    }

    // 1. Read null column
    if (_null_iter != nullptr) {
        RETURN_IF_ERROR(_null_iter->next_batch(n, null_column));
        down_cast<vectorized::NullableColumn*>(dst)->update_has_null();
    }

    // Read all fields
    // TODO(Alvin): _field_iters may have different order with struct_column
    // FIXME:
    for (int i = 0; i < _field_iters.size(); ++i) {
        auto num_to_read = *n;
        RETURN_IF_ERROR(_field_iters[i]->next_batch(&num_to_read, struct_column->fields()[i].get()));
    }

    return Status::OK();
}

Status StructColumnIterator::next_batch(const vectorized::SparseRange& range, vectorized::Column* dst) {
    vectorized::StructColumn* struct_column = nullptr;
    vectorized::NullColumn* null_column = nullptr;
    if (dst->is_nullable()) {
        auto* nullable_column = down_cast<vectorized::NullableColumn*>(dst);

        struct_column = down_cast<vectorized::StructColumn*>(nullable_column->data_column().get());
        null_column = down_cast<vectorized::NullColumn*>(nullable_column->null_column().get());
    } else {
        struct_column = down_cast<vectorized::StructColumn*>(dst);
    }
    // Read null column
    if (_null_iter != nullptr) {
        RETURN_IF_ERROR(_null_iter->next_batch(range, null_column));
        down_cast<vectorized::NullableColumn*>(dst)->update_has_null();
    }
    // Read all fields
    for (int i = 0; i < _field_iters.size(); ++i) {
        RETURN_IF_ERROR(_field_iters[i]->next_batch(range, struct_column->fields()[i].get()));
    }
    return Status::OK();
}

Status StructColumnIterator::fetch_values_by_rowid(const rowid_t* rowids, size_t size, vectorized::Column* values) {
    vectorized::StructColumn* struct_column = nullptr;
    vectorized::NullColumn* null_column = nullptr;
    // 1. Read null column
    if (_null_iter != nullptr) {
        auto* nullable_column = down_cast<vectorized::NullableColumn*>(values);
        struct_column = down_cast<vectorized::StructColumn*>(nullable_column->data_column().get());
        null_column = down_cast<vectorized::NullColumn*>(nullable_column->null_column().get());
        RETURN_IF_ERROR(_null_iter->fetch_values_by_rowid(rowids, size, null_column));
        nullable_column->update_has_null();
    } else {
        struct_column = down_cast<vectorized::StructColumn*>(values);
    }
    if (_null_iter != nullptr) {
        RETURN_IF_ERROR(_null_iter->fetch_values_by_rowid(rowids, size, null_column));
    }

    // read all fields
    for (int i = 0; i < _field_iters.size(); ++i) {
        auto& col = struct_column->fields()[i];
        RETURN_IF_ERROR(_field_iters[i]->fetch_values_by_rowid(rowids, size, col.get()));
    }
    return Status::OK();
}

Status StructColumnIterator::seek_to_first() {
    if (_null_iter != nullptr) {
        RETURN_IF_ERROR(_null_iter->seek_to_first());
    }
    for (auto& iter : _field_iters) {
        RETURN_IF_ERROR(iter->seek_to_first());
    }
    return Status::OK();
}

Status StructColumnIterator::seek_to_ordinal(ordinal_t ord) {
    if (_null_iter != nullptr) {
        RETURN_IF_ERROR(_null_iter->seek_to_ordinal(ord));
    }
    for (auto& iter : _field_iters) {
        RETURN_IF_ERROR(iter->seek_to_ordinal(ord));
    }
    return Status::OK();
}

} // namespace starrocks
