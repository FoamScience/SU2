/*!
 * \file C2DContainer.hpp
 * \brief A templated vector/matrix object.
 * \author P. Gomes
 * \version 7.0.0 "Blackbird"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation
 * (http://su2foundation.org)
 *
 * Copyright 2012-2019, SU2 Contributors (cf. AUTHORS.md)
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "allocation_toolbox.hpp"
#include "../datatype_structure.hpp"

#include <utility>

/*!
 * \enum StorageType
 * \brief Supported ways to flatten a matrix into an array.
 * Contiguous rows or contiguous columns respectively.
 */
enum class StorageType {RowMajor=0, ColumnMajor=1};

/*!
 * \enum SizeType
 * \brief Special value "DynamicSize" to indicate a dynamic size.
 */
enum SizeType : size_t {DynamicSize=0};


/*--- Namespace to "hide" helper classes and
 functions used by the container class. ---*/
namespace container_helpers
{
/*!
 * \class AccessorImpl
 * \brief Base accessor class and version of template for both sizes known at compile time.
 *
 * The actual container inherits from this class, this is to reduce the number of
 * methods that need to be redefined with each size specialization.
 */
template<typename Index_t, class Scalar_t, StorageType Store, size_t AlignSize, size_t StaticRows, size_t StaticCols>
class AccessorImpl
{
  static_assert(!(StaticRows==1 && Store==StorageType::ColumnMajor),
                "Row vector should have row-major storage.");
  static_assert(!(StaticCols==1 && Store==StorageType::RowMajor),
                "Column vector should have column-major storage.");
protected:
  /*!
   * For static specializations AlignSize will force the alignment
   * specification of the entire class, not just the data.
   */
  alignas(AlignSize) Scalar_t m_data[StaticRows*StaticCols];

  /*!
   * Static size specializations use this do-nothing allocation macro.
   */
#define DUMMY_ALLOCATOR \
  void m_allocate(size_t sz, Index_t rows, Index_t cols) noexcept {}
  /*!
   * Dynamic size specializations use this one, EXTRA is used to set some
   * runtime internal value that depend on the number of rows/columns.
   * What values need setting depends on the specialization as not all have
   * members for e.g. number of rows and cols (static size optimization).
   */
#define REAL_ALLOCATOR(EXTRA)                                           \
  static_assert(MemoryAllocation::is_power_of_two(AlignSize),           \
                "AlignSize is not a power of two.");                    \
                                                                        \
  void m_allocate(size_t sz, Index_t rows, Index_t cols) noexcept {     \
    EXTRA;                                                              \
    m_data = MemoryAllocation::aligned_alloc<Scalar_t>(AlignSize,sz);   \
  }

  DUMMY_ALLOCATOR

public:
  /*!
   * Dynamic types need to manage internal data as the derived class would
   * not compile if it tried to set m_data to null on static specializations.
   * Move construct/assign are enabled by transferring ownership of data
   * pointer, the rvalue is left in the empty state.
   * The default ctor needs to "INIT" some fields. The move ctor/assign need
   * to "MOVE" those fields, i.e. copy and set "other" appropriately.
   */
#define CUSTOM_CTOR_AND_DTOR_BASE(INIT,MOVE)                            \
  AccessorImpl() noexcept : m_data(nullptr) {INIT;}                     \
                                                                        \
  AccessorImpl(AccessorImpl&& other) noexcept                           \
  {                                                                     \
    MOVE; m_data=other.m_data; other.m_data=nullptr;                    \
  }                                                                     \
                                                                        \
  AccessorImpl& operator= (AccessorImpl&& other) noexcept               \
  {                                                                     \
    if(m_data!=nullptr) free(m_data);                                   \
    MOVE; m_data=other.m_data; other.m_data=nullptr;                    \
    return *this;                                                       \
  }                                                                     \
                                                                        \
  ~AccessorImpl()                                                       \
  {                                                                     \
    if(m_data!=nullptr)                                                 \
      MemoryAllocation::aligned_free<Scalar_t>(m_data);                 \
  }
  /*!
   * Shorthand for when specialization has only one more member than m_data.
   */
#define CUSTOM_CTOR_AND_DTOR(X) \
  CUSTOM_CTOR_AND_DTOR_BASE(X=0, X=other.X; other.X=0)

  /*!
   * Universal accessors return a raw pointer to the data.
   */
#define UNIV_ACCESSORS                                                  \
  bool empty() const noexcept {return size()==0;}                       \
  Scalar_t* data() noexcept {return m_data;}                            \
  const Scalar_t* data() const noexcept {return m_data;}

  /*!
   * Operator (,) gives pointwise access, operator [] returns a pointer to the
   * first element of the row/column of a row/column-major matrix respectively.
   */
#define MATRIX_ACCESSORS(M,N)                                           \
  UNIV_ACCESSORS                                                        \
  Index_t rows() const noexcept {return M;}                             \
  Index_t cols() const noexcept {return N;}                             \
  size_t size() const noexcept {return M*N;}                            \
                                                                        \
  const Scalar_t& operator() (const Index_t i,                          \
                              const Index_t j) const noexcept           \
  {                                                                     \
    assert(i>=0 && i<M && j>=0 && j<N);                                 \
    return m_data[(Store==StorageType::RowMajor)? i*N+j : i+j*M];       \
  }                                                                     \
                                                                        \
  Scalar_t& operator() (const Index_t i, const Index_t j) noexcept      \
  {                                                                     \
    const AccessorImpl& const_this = *this;                             \
    return const_cast<Scalar_t&>( const_this(i,j) );                    \
  }                                                                     \
                                                                        \
  const Scalar_t* operator[] (const Index_t k) const noexcept           \
  {                                                                     \
    if(Store == StorageType::RowMajor) {                                \
      assert(k>=0 && k<M);                                              \
      return &m_data[k*N];                                              \
    } else {                                                            \
      assert(k>=0 && k<N);                                              \
      return &m_data[k*M];                                              \
    }                                                                   \
  }                                                                     \
                                                                        \
  Scalar_t* operator[] (const Index_t k) noexcept                       \
  {                                                                     \
    const AccessorImpl& const_this = *this;                             \
    return const_cast<Scalar_t*>( const_this[k] );                      \
  }

  /*!
   * Vectors do not provide operator [] as it is redundant
   * since operator () already returns by reference.
   */
#define VECTOR_ACCESSORS(M,ROWMAJOR)                                    \
  UNIV_ACCESSORS                                                        \
  Index_t rows() const noexcept {return ROWMAJOR? 1 : M;}               \
  Index_t cols() const noexcept {return ROWMAJOR? M : 1;}               \
  size_t  size() const noexcept {return M;}                             \
                                                                        \
  Scalar_t& operator() (const Index_t i) noexcept                       \
  {                                                                     \
    assert(i>=0 && i<M);                                                \
    return m_data[i];                                                   \
  }                                                                     \
                                                                        \
  const Scalar_t& operator() (const Index_t i) const noexcept           \
  {                                                                     \
    assert(i>=0 && i<M);                                                \
    return m_data[i];                                                   \
  }

  MATRIX_ACCESSORS(StaticRows,StaticCols)
};

/*!
 * Specialization for compile-time number of columns.
 */
template<typename Index_t, class Scalar_t, StorageType Store, size_t AlignSize, size_t StaticCols>
class AccessorImpl<Index_t,Scalar_t,Store,AlignSize,DynamicSize,StaticCols>
{
  static_assert(!(StaticCols==1 && Store==StorageType::RowMajor),
                "Column vector should have column-major storage.");
protected:
  Index_t m_rows;
  Scalar_t* m_data;

  REAL_ALLOCATOR(m_rows=rows)

public:
  CUSTOM_CTOR_AND_DTOR(m_rows)

  MATRIX_ACCESSORS(m_rows,StaticCols)
};

/*!
 * Specialization for compile-time number of columns.
 */
template<typename Index_t, class Scalar_t, StorageType Store, size_t AlignSize, size_t StaticRows>
class AccessorImpl<Index_t,Scalar_t,Store,AlignSize,StaticRows,DynamicSize>
{
  static_assert(!(StaticRows==1 && Store==StorageType::ColumnMajor),
                "Row vector should have row-major storage.");
protected:
  Index_t m_cols;
  Scalar_t* m_data;

  REAL_ALLOCATOR(m_cols=cols)

public:
  CUSTOM_CTOR_AND_DTOR(m_cols)

  MATRIX_ACCESSORS(StaticRows,m_cols)
};

/*!
 * Specialization for fully dynamic sizes (generic matrix).
 */
template<typename Index_t, class Scalar_t, StorageType Store, size_t AlignSize>
class AccessorImpl<Index_t,Scalar_t,Store,AlignSize,DynamicSize,DynamicSize>
{
protected:
  Index_t m_rows, m_cols;
  Scalar_t* m_data;

  REAL_ALLOCATOR(m_rows=rows; m_cols=cols)

public:
  CUSTOM_CTOR_AND_DTOR_BASE(m_rows = 0; m_cols = 0,
                            m_rows = other.m_rows; other.m_rows = 0;
                            m_cols = other.m_cols; other.m_cols = 0)

  MATRIX_ACCESSORS(m_rows,m_cols)
};

/*!
 * Specialization for static column-vector.
 */
template<typename Index_t, class Scalar_t, size_t AlignSize, size_t StaticRows>
class AccessorImpl<Index_t,Scalar_t,StorageType::ColumnMajor,AlignSize,StaticRows,1>
{
protected:
  alignas(AlignSize) Scalar_t m_data[StaticRows];

  DUMMY_ALLOCATOR

public:
  VECTOR_ACCESSORS(StaticRows,false)
};

/*!
 * Specialization for dynamic column-vector.
 */
template<typename Index_t, class Scalar_t, size_t AlignSize>
class AccessorImpl<Index_t,Scalar_t,StorageType::ColumnMajor,AlignSize,DynamicSize,1>
{
protected:
  Index_t m_rows;
  Scalar_t* m_data;

  REAL_ALLOCATOR(m_rows=rows)

public:
  CUSTOM_CTOR_AND_DTOR(m_rows)

  VECTOR_ACCESSORS(m_rows,false)
};

/*!
 * Specialization for static row-vector.
 */
template<typename Index_t, class Scalar_t, size_t AlignSize, size_t StaticCols>
class AccessorImpl<Index_t,Scalar_t,StorageType::RowMajor,AlignSize,1,StaticCols>
{
protected:
  alignas(AlignSize) Scalar_t m_data[StaticCols];

  DUMMY_ALLOCATOR

public:
  VECTOR_ACCESSORS(StaticCols,true)
};

/*!
 * Specialization for dynamic row-vector.
 */
template<typename Index_t, class Scalar_t, size_t AlignSize>
class AccessorImpl<Index_t,Scalar_t,StorageType::RowMajor,AlignSize,1,DynamicSize>
{
protected:
  Index_t m_cols;
  Scalar_t* m_data;

  REAL_ALLOCATOR(m_cols=cols)

public:
  CUSTOM_CTOR_AND_DTOR(m_cols)

  VECTOR_ACCESSORS(m_cols,true)
};

#undef CUSTOM_CTOR_AND_DTOR_BASE
#undef CUSTOM_CTOR_AND_DTOR
#undef DUMMY_ALLOCATOR
#undef REAL_ALLOCATOR
#undef MATRIX_ACCESSORS
#undef VECTOR_ACCESSORS
#undef UNIV_ACCESSORS
}

/*!
 * \class C2DContainer
 * \brief A templated matrix/vector-like object.
 *
 * See notes about MATRIX_ACCESSORS and VECTOR_ACCESSORS for how to access data.
 * For how to construct/resize/initialize see methods below.
 *
 * Template parameters:
 *
 * \param Index_t    - The data type (built-in) for indices, signed and unsigned allowed.
 * \param Scalar_t   - The stored data type, anything that can be default constructed.
 * \param Store      - Mode to map 1D to 2D, row-major or column-major.
 * \param AlignSize  - Desired alignment for the data in bytes, 0 means default.
 * \param StaticRows - Number of rows at compile time, for dynamic (sized at runtime) use "DynamicSize".
 * \param StaticCols - As above for columns.
 *
 * \note All accesses to data are range checked via assertions, for
 * release compile with -DNDEBUG to avoid the associated overhead.
 *
 */
template<typename Index_t, class Scalar_t, StorageType Store, size_t AlignSize, size_t StaticRows, size_t StaticCols>
class C2DContainer :
  public container_helpers::AccessorImpl<Index_t,Scalar_t,Store,AlignSize,StaticRows,StaticCols>
{
  static_assert(is_integral<Index_t>::value,"");

private:
  using Base = container_helpers::AccessorImpl<Index_t,Scalar_t,Store,AlignSize,StaticRows,StaticCols>;
  using Base::m_data;
  using Base::m_allocate;
public:
  using Base::size;

private:
  /*!
   * \brief Logic to resize data according to arguments, a non DynamicSize cannot be changed.
   */
  size_t m_resize(Index_t rows, Index_t cols) noexcept
  {
    /*--- fully static, no allocation needed ---*/
    if(StaticRows!=DynamicSize && StaticCols!=DynamicSize)
        return StaticRows*StaticCols;

    /*--- dynamic row vector, swap size specification ---*/
    if(StaticRows==1 && StaticCols==DynamicSize) {cols = rows; rows = 1;}

    /*--- assert a static size is not being asked to change ---*/
    if(StaticRows!=DynamicSize) assert(rows==StaticRows && "A static size was asked to change.");
    if(StaticCols!=DynamicSize) assert(cols==StaticCols && "A static size was asked to change.");

    /*--- "rectify" sizes before continuing as asserts are usually dissabled ---*/
    rows = (StaticRows!=DynamicSize)? StaticRows : rows;
    cols = (StaticCols!=DynamicSize)? StaticCols : cols;

    /*--- number of requested elements ---*/
    size_t reqSize = rows*cols;

    /*--- compare with current dimensions to determine if deallocation
     is needed, also makes the container safe against self assignment
     no need to check for 0 size as the allocators handle that ---*/
    if(m_data!=nullptr)
    {
      if(rows==this->rows() && cols==this->cols())
        return reqSize;
      free(m_data);
    }

    /*--- round up size to a multiple of the alignment specification if necessary ---*/
    size_t bytes = reqSize*sizeof(Scalar_t);
    size_t allocSize = (AlignSize==0)? bytes : ((bytes+AlignSize-1)/AlignSize)*AlignSize;

    /*--- request actual allocation to base class as it needs specialization ---*/
    m_allocate(allocSize,rows,cols);

    return reqSize;
  }

public:
  /*!
   * \brief Default ctor.
   */
  C2DContainer() noexcept : Base() {}

  /*!
   * \brief Sizing ctor (no initialization of data).
   * For matrices size1 is rows and size2 columns, for vectors size1 is lenght and size2 is ignored.
   */
  C2DContainer(const Index_t size1, const Index_t size2 = 1) noexcept : Base()
  {
    m_resize(size1,size2);
  }

  /*!
   * \brief Copy ctor.
   */
  C2DContainer(const C2DContainer& other) noexcept : Base()
  {
    size_t sz = m_resize(other.rows(),other.cols());
    for(size_t i=0; i<sz; ++i) m_data[i] = other.m_data[i];
  }

  /*!
   * \brief Copy assignment operator, no re-allocation if sizes do not change.
   */
  C2DContainer& operator= (const C2DContainer& other) noexcept
  {
    size_t sz = m_resize(other.rows(),other.cols());
    for(size_t i=0; i<sz; ++i) m_data[i] = other.m_data[i];
    return *this;
  }

  /*!
   * \brief Move ctor, implemented by base class (if fully static works as copy).
   */
  C2DContainer(C2DContainer&&) noexcept = default;

  /*!
   * \brief Move assign operator, implemented by base class (if fully static works as copy).
   */
  C2DContainer& operator= (C2DContainer&&) noexcept = default;

  /*!
   * \overload Set all entries to rhs value (syntax sugar, see "resize").
   */
  C2DContainer& operator= (const Scalar_t& value) noexcept
  {
    setConstant(value);
    return *this;
  }

  /*!
   * \brief Request a change of size.
   * \note Providing an interface that allowed resizing with value could cause
   * overloading ambiguities when Scalar_t = Index_t, so we do not. But it
   * is possible to assign a constant on top of resize: A.resize(m,n) = x;
   */
  C2DContainer& resize(const Index_t size1, const Index_t size2 = 1) noexcept
  {
    m_resize(size1,size2);
    return *this;
  }

  /*!
   * \brief Set value of all entries to "value".
   */
  void setConstant(const Scalar_t& value) noexcept
  {
    for(size_t i=0; i<size(); ++i) m_data[i] = value;
  }
};


/*!
 * \brief Useful typedefs with default template parameters
 */
template<class T> using su2vector = C2DContainer<unsigned long, T, StorageType::ColumnMajor, 64, DynamicSize, 1>;
template<class T> using su2matrix = C2DContainer<unsigned long, T, StorageType::RowMajor,    64, DynamicSize, DynamicSize>;

using su2activevector = su2vector<su2double>;
using su2activematrix = su2matrix<su2double>;

using su2passivevector = su2vector<passivedouble>;
using su2passivematrix = su2matrix<passivedouble>;
