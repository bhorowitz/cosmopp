#ifndef COSMO_PP_MATRIX_IMPL_HPP
#define COSMO_PP_MATRIX_IMPL_HPP

#ifdef COSMO_OMP
#include <omp.h>
#endif

#include <vector>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>

#include <macros.hpp>
#include <exception_handler.hpp>
#include <matrix.hpp>

namespace Math
{

template<typename T>
Matrix<T>::Matrix(int rows, int cols)
{
    check(rows >= 0, "");
    check(cols >= 0, "");

    rows_ = rows;
    cols_ = cols;
    v_.resize(rows_ * cols_);
}

template<typename T>
Matrix<T>::Matrix(int rows, int cols, DataType val)
{
    check(rows >= 0, "");
    check(cols >= 0, "");

    rows_ = rows;
    cols_ = cols;
    v_.resize(rows_ * cols_, val);
}

template<typename T>
Matrix<T>::Matrix(const Matrix<DataType>& other)
{
    rows_ = other.rows_;
    cols_ = other.cols_;
    v_.resize(rows_ * cols_);
#pragma omp parallel for default(shared)
    for(int i = 0; i < rows_; ++i)
    {
        for(int j = 0; j < cols_; ++j)
            v_[i * cols_ + j] = other(i, j);
    }
}

template<typename T>
Matrix<T>::Matrix(const std::vector<DataType>& vec, bool columnVector)
{
    v_ = vec;
    if(columnVector)
    {
        rows_ = vec.size();
        cols_ = 1;
    }
    else
    {
        rows_ = 1;
        cols_ = vec.size();
    }
}

template<typename T>
void
Matrix<T>::resize(int rows, int cols)
{
    check(rows >= 0, "");
    check(cols >= 0, "");

    rows_ = rows;
    cols_ = cols;
    v_.clear();
    v_.resize(rows_ * cols_);
}

template<typename T>
void
Matrix<T>::resize(int rows, int cols, DataType val)
{
    check(rows >= 0, "");
    check(cols >= 0, "");

    rows_ = rows;
    cols_ = cols;
    v_.clear();
    v_.resize(rows_ * cols_, val);
}

template<typename T>
const T&
Matrix<T>::operator()(int i, int j) const
{
    checkIndices(i, j);
    return v_[i * cols_ + j];
}

template<typename T>
T&
Matrix<T>::operator()(int i, int j)
{
    checkIndices(i, j);
    return v_[i * cols_ + j];
}

template<typename T>
void
Matrix<T>::checkIndices(int i, int j) const
{
    check(i >= 0 && i < rows(), "invalid index i = " << i << ", should be non-negative and less than " << rows());
    check(j >= 0 && j < cols(), "invalid index j = " << j << ", should be non-negative and less than " << cols());
}

template<typename T>
void
Matrix<T>::writeIntoFile(const char* fileName) const
{
    std::ofstream out(fileName, std::ios::binary | std::ios::out);
    StandardException exc;
    if(!out)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot write into output file " << fileName;
        exc.set(exceptionStr.str());
        throw exc;
    }

    out.write((char*)(&rows_), sizeof(rows_));
    out.write((char*)(&cols_), sizeof(cols_));
    out.write((char*)(&(v_[0])), v_.size() * sizeof(DataType));
    out.close();
}

template<typename T>
void
Matrix<T>::readFromFile(const char* fileName)
{
    std::ifstream in(fileName, std::ios::binary | std::ios::in);
    StandardException exc;
    if(!in)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot read from file " << fileName;
        exc.set(exceptionStr.str());
        throw exc;
    }

    in.read((char*)(&rows_), sizeof(rows_));
    if(rows_ < 0)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Invalid number of rows " << rows_ << " in the file " << fileName << ". Must be non-negative.";
        exc.set(exceptionStr.str());
        throw exc;
    }

    in.read((char*)(&cols_), sizeof(cols_));
    if(cols_ < 0)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Invalid number of columns " << cols_ << " in the file " << fileName << ". Must be non-negative.";
        exc.set(exceptionStr.str());
        throw exc;
    }

    v_.clear();
    v_.resize(rows_ * cols_);

    in.read((char*)(&(v_[0])), v_.size() * sizeof(DataType));
    in.close();
}

template<typename T>
void
Matrix<T>::writeIntoTextFile(const char* fileName, int precision) const
{
    std::ofstream out(fileName);
    StandardException exc;
    if(!out)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot write into output file " << fileName;
        exc.set(exceptionStr.str());
        throw exc;
    }

    out << rows_ << '\t' << cols_ << std::endl;
    if(rows_ == 0 || cols_ == 0)
    {
        out.close();
        return;
    }

    check(precision >= 0, "");
    out << std::setprecision(precision);

    for(int i = 0; i < rows_; ++i)
    {
        for(int j = 0; j < cols_ - 1; ++j)
            out << (*this)(i, j) << '\t';
        out << (*this)(i, cols_ - 1) << std::endl;
    }
    out.close();
}

template<typename T>
void
Matrix<T>::readFromTextFile(const char* fileName)
{
    std::ifstream in(fileName);
    StandardException exc;
    if(!in)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot read from file " << fileName;
        exc.set(exceptionStr.str());
        throw exc;
    }

    in >> rows_ >> cols_;
    if(rows_ < 0 || cols_ < 0)
    {
        std::stringstream exceptionStr;
        exceptionStr << "The number of rows and columns must be non-negative! Read " << rows_ << " and " << cols_ << " from " << fileName;
        exc.set(exceptionStr.str());
        throw exc;
    }

    v_.clear();
    v_.resize(rows_ * cols_);
    for(int i = 0; i < rows_; ++i)
    {
        for(int j = 0; j < cols_; ++j)
            in >> (*this)(i, j);
    }
    in.close();
}

template<typename T>
Matrix<T>
Matrix<T>::getRow(int i) const
{
    check(i >= 0 && i < rows_, "");
    Matrix<DataType> res(1, cols_);
#pragma omp parallel for default(shared)
    for(int j = 0; j < cols_; ++j)
        res(0, j) = (*this)(i, j);
    return res;
}

template<typename T>
Matrix<T>
Matrix<T>::getCol(int i) const
{
    check(i >= 0 && i < cols_, "");
    Matrix<DataType> res(rows_, 1);
#pragma omp parallel for default(shared)
    for(int j = 0; j < rows_; ++j)
        res(j, 0) = (*this)(j, i);
    return res;
}

template<typename T>
void
Matrix<T>::copy(const Matrix<DataType>& other)
{
    if(&other == this)
        return;

    resize(other.rows(), other.cols());
#pragma omp parallel for default(shared)
    for(int i = 0; i < rows_; ++i)
    {
        for(int j = 0; j < cols_; ++j)
            (*this)(i, j) = other(i, j);
    }
}

template<typename T>
void
Matrix<T>::add(const Matrix<DataType>& other)
{
    check(rows_ == other.rows_, "cannot add matrices of different sizes");
    check(cols_ == other.cols_, "cannot add matrices of different sizes");

#pragma omp parallel for default(shared)
    for(int i = 0; i < rows_; ++i)
    {
        for(int j = 0; j < cols_; ++j)
            (*this)(i, j) += other(i, j);
    }
}

template<typename T>
void
Matrix<T>::subtract(const Matrix<DataType>& other)
{
    check(rows_ == other.rows_, "cannot subtract matrices of different sizes");
    check(cols_ == other.cols_, "cannot subtract matrices of different sizes");

#pragma omp parallel for default(shared)
    for(int i = 0; i < rows_; ++i)
    {
        for(int j = 0; j < cols_; ++j)
            (*this)(i, j) -= other(i, j);
    }
}

template<typename T>
void
Matrix<T>::getTranspose(Matrix<DataType>* res) const
{
    res->resize(cols_, rows_);
    for(int i = 0; i < rows_; ++i)
        for(int j = 0; j < cols_; ++j)
            (*res)(j, i) = (*this)(i, j);
}

template<typename T>
void
Matrix<T>::multiplyMatrices(const Matrix<DataType>& a, const Matrix<DataType>& b, Matrix<DataType>* res)
{
    check(a.cols_ == b.rows_, "invalid multiplication, a must have the same number of columns as b rows");

    check(!res->isSymmetric(), "the product of two matrices is not necessarily symmetric, even if both are");

    res->resize(a.rows_, b.cols_);

#pragma omp parallel for default(shared)
    for(int i = 0; i < a.rows_; ++i)
    {
        for(int j = 0; j < b.cols_; ++j)
        {
            DataType x = 0;
            for(int k = 0; k < a.cols_; ++k)
                x += a(i, k) * b(k, j);
            (*res)(i, j) = x;
        }
    }
}

#ifdef COSMO_LAPACK

template<>
void
Matrix<double>::multiplyMatrices(const Matrix<double>& a, const Matrix<double>& b, Matrix<double>* res);

#endif

#ifdef COSMO_LAPACK

template<typename T>
int
Matrix<T>::luFactorize(std::vector<int>* pivot)
{
    check(false, "");
    return -1;
}

template<typename T>
int
Matrix<T>::invertFromLUFactorization(std::vector<int>* pivot)
{
    check(false, "");
    return -1;
}

template<typename T>
int
Matrix<T>::invert()
{
    check(false, "");
    return -1;
}

template<typename T>
int
Matrix<T>::getInverse(Matrix<T>* res) const
{
    check(false, "");
    return -1;
}

template<typename T>
double
Matrix<T>::determinantFromLUFactorization(std::vector<int>* pivot) const
{
    check(false, "");
    return 0;
}

template<typename T>
double
Matrix<T>::determinant() const
{
    check(false, "");
    return 0;
}

template<typename T>
double
Matrix<T>::logDetFromLUFactorization(std::vector<int>* pivot, int* sign) const
{
    check(false, "");
    return 0;
}

template<typename T>
double
Matrix<T>::logDet(int* sign) const
{
    check(false, "");
    return 0;
}

template<>
int
Matrix<double>::luFactorize(std::vector<int>* pivot);

template<>
int
Matrix<double>::invertFromLUFactorization(std::vector<int>* pivot);

template<>
int
Matrix<double>::invert();

template<>
int
Matrix<double>::getInverse(Matrix<double>* res) const;

template<>
double
Matrix<double>::determinantFromLUFactorization(std::vector<int>* pivot) const;

template<>
double
Matrix<double>::determinant() const;

template<>
double
Matrix<double>::logDetFromLUFactorization(std::vector<int>* pivot, int* sign) const;

template<>
double
Matrix<double>::logDet(int* sign) const;

#endif

template<typename T>
SymmetricMatrix<T>::SymmetricMatrix(int rows, int cols) : BaseType()
{
    check(rows >= 0, "");
    check(rows == cols, "symmetric matrix must have rows = cols");

    rows_ = rows;
    cols_ = cols;
    v_.resize(rows_ * (rows_ + 1) / 2);
}

template<typename T>
SymmetricMatrix<T>::SymmetricMatrix(int rows, int cols, DataType val) : BaseType()
{
    check(rows >= 0, "");
    check(rows == cols, "symmetric matrix must have rows = cols");

    rows_ = rows;
    cols_ = cols;
    v_.resize(rows_ * (rows_ + 1) / 2, val);
}

template<typename T>
SymmetricMatrix<T>::SymmetricMatrix(const SymmetricMatrix<DataType>& other) : BaseType()
{
    rows_ = other.rows_;
    cols_ = other.cols_;
    check(rows_ == cols_, "");

    v_ = other.v_;
}

template<typename T>
const T&
SymmetricMatrix<T>::operator()(int i, int j) const
{
    checkIndices(i, j);
    if(i < j)
        std::swap(i, j);

    return v_[i * (i + 1) / 2 + j];
}

template<typename T>
T&
SymmetricMatrix<T>::operator()(int i, int j)
{
    return const_cast<T&>(static_cast<const SymmetricMatrix<T>& >(*this)(i, j));
}

template<typename T>
void
SymmetricMatrix<T>::resize(int rows, int cols) 
{
    check(rows >= 0, "");
    check(rows == cols, "symmetric matrix must have rows = cols");

    rows_ = rows;
    cols_ = cols;
    v_.clear();
    v_.resize(rows_ * (rows_ + 1) / 2);
}

template<typename T>
void
SymmetricMatrix<T>::resize(int rows, int cols, DataType val) 
{
    check(rows >= 0, "");
    check(rows == cols, "symmetric matrix must have rows = cols");

    rows_ = rows;
    cols_ = cols;
    v_.clear();
    v_.resize(rows_ * (rows_ + 1) / 2, val);
}

template<typename T>
void
SymmetricMatrix<T>::writeIntoFile(const char* fileName) const
{
    std::ofstream out(fileName, std::ios::binary | std::ios::out);
    StandardException exc;
    if(!out)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot write into output file " << fileName;
        exc.set(exceptionStr.str());
        throw exc;
    }

    check(rows_ == cols_, "");

    out.write((char*)(&rows_), sizeof(rows_));
    out.write((char*)(&(v_[0])), v_.size() * sizeof(DataType));
    out.close();
}

template<typename T>
void
SymmetricMatrix<T>::readFromFile(const char* fileName)
{
    std::ifstream in(fileName, std::ios::binary | std::ios::in);
    StandardException exc;
    if(!in)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot read from file " << fileName;
        exc.set(exceptionStr.str());
        throw exc;
    }

    in.read((char*)(&rows_), sizeof(rows_));
    if(rows_ < 0)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Invalid matrix size " << rows_ << " in the file " << fileName << ". Must be non-negative.";
        exc.set(exceptionStr.str());
        throw exc;
    }

    cols_ = rows_;

    v_.clear();
    v_.resize(rows_ * (rows_ + 1) / 2);

    in.read((char*)(&(v_[0])), v_.size() * sizeof(DataType));
    in.close();
}

template<typename T>
void
SymmetricMatrix<T>::writeIntoTextFile(const char* fileName, int precision) const
{
    std::ofstream out(fileName);
    StandardException exc;
    if(!out)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot write into output file " << fileName;
        exc.set(exceptionStr.str());
        throw exc;
    }

    check(rows_ == cols_, "");
    out << rows_ << '\t' << cols_ << std::endl;
    if(rows_ == 0)
    {
        out.close();
        return;
    }

    check(precision >= 0, "");
    out << std::setprecision(precision);

    for(int i = 0; i < rows_; ++i)
    {
        for(int j = 0; j < rows_ - 1; ++j)
            out << (*this)(i, j) << '\t';
        out << (*this)(i, rows_ - 1) << std::endl;
    }
    out.close();
}

template<typename T>
void
SymmetricMatrix<T>::readFromTextFile(const char* fileName)
{
    std::ifstream in(fileName);
    StandardException exc;
    if(!in)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot read from file " << fileName;
        exc.set(exceptionStr.str());
        throw exc;
    }

    in >> rows_ >> cols_;
    if(rows_ < 0)
    {
        std::stringstream exceptionStr;
        exceptionStr << "The matrix size must be non-negative! Read " << rows_ << " from " << fileName;
        exc.set(exceptionStr.str());
        throw exc;
    }

    if(rows_ != cols_)
    {
        std::stringstream exceptionStr;
        exceptionStr << "The matrix in the file " << fileName << " is not symmetric.";
        exc.set(exceptionStr.str());
        throw exc;
    }

    v_.clear();
    v_.resize(rows_ * (rows_ + 1) / 2);
    for(int i = 0; i < rows_; ++i)
    {
        DataType x;
        for(int j = 0; j < i; ++j)
        {
            in >> x;
            if(x != (*this)(j, i))
            {
                std::stringstream exceptionStr;
                exceptionStr << "The matrix in the file is not symmetric! The element (" << i << "," << j << ") is " << x << " while the element (" << j << "," << i << ") is " << (*this)(j, i);
            }
        }
        for(int j = i; j < rows_; ++j)
            in >> (*this)(i, j);
    }
    in.close();
}

template<typename T>
void
SymmetricMatrix<T>::copy(const Matrix<DataType>& other)
{
    if(&other == this)
        return;
    
    check(other.isSymmetric(), "cannot copy from non-symmetric matrix to symmetric");

    resize(other.rows(), other.cols());
#pragma omp parallel for default(shared)
    for(int i = 0; i < rows_; ++i)
    {
        for(int j = i; j < cols_; ++j)
            (*this)(i, j) = other(i, j);
    }
}

template<typename T>
void
SymmetricMatrix<T>::add(const Matrix<DataType>& other)
{
    check(rows_ == other.rows(), "cannot add matrices of different sizes");
    check(cols_ == other.cols(), "cannot add matrices of different sizes");
    check(other.isSymmetric(), "cannot add non-symmetric matrix to symmetric");

#pragma omp parallel for default(shared)
    for(int i = 0; i < rows_; ++i)
    {
        for(int j = i; j < cols_; ++j)
            (*this)(i, j) += other(i, j);
    }
}


template<typename T>
void
SymmetricMatrix<T>::subtract(const Matrix<DataType>& other)
{
    check(rows_ == other.rows(), "cannot subtract matrices of different sizes");
    check(cols_ == other.cols(), "cannot subtract matrices of different sizes");
    check(other.isSymmetric(), "cannot subtract non-symmetric matrix from symmetric");

#pragma omp parallel for default(shared)
    for(int i = 0; i < rows_; ++i)
    {
        for(int j = i; j < cols_; ++j)
            (*this)(i, j) -= other(i, j);
    }
}

#ifdef COSMO_LAPACK

template<typename T>
int
SymmetricMatrix<T>::choleskyFactorize()
{
    check(false, "");
    return -1;
}

template<typename T>
int
SymmetricMatrix<T>::invertFromCholeskyFactorization()
{
    check(false, "");
    return -1;
}

template<typename T>
int
SymmetricMatrix<T>::invert()
{
    check(false, "");
    return -1;
}

template<typename T>
double
SymmetricMatrix<T>::determinantFromCholeskyFactorization() const
{
    check(false, "");
    return 0;
}

template<typename T>
double
SymmetricMatrix<T>::determinant() const
{
    check(false, "");
    return 0;
}

template<typename T>
double
SymmetricMatrix<T>::logDetFromCholeskyFactorization(int* sign) const
{
    check(false, "");
    return 0;
}

template<typename T>
double
SymmetricMatrix<T>::logDet(int* sign) const
{
    check(false, "");
    return 0;
}

template<typename T>
int
SymmetricMatrix<T>::getEigen(std::vector<double>* eigenvals, Matrix<double>* eigenvecs, bool positiveDefinite) const
{
    check(false, "");
    return -1;
}

template<>
int
SymmetricMatrix<double>::choleskyFactorize();

template<>
int
SymmetricMatrix<double>::invertFromCholeskyFactorization();

template<>
int
SymmetricMatrix<double>::invert();

template<>
double
SymmetricMatrix<double>::determinantFromCholeskyFactorization() const;

template<>
double
SymmetricMatrix<double>::determinant() const;

template<>
double
SymmetricMatrix<double>::logDetFromCholeskyFactorization(int* sign) const;

template<>
double
SymmetricMatrix<double>::logDet(int* sign) const;

template<>
int
SymmetricMatrix<double>::getEigen(std::vector<double>* eigenvals, Matrix<double>* eigenvecs, bool positiveDefinite) const;


#endif

} // namespace Math


#endif

