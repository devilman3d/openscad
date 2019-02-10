// Copyright (c) 1999,2003,2004  
// Utrecht University (The Netherlands),
// ETH Zurich (Switzerland),
// INRIA Sophia-Antipolis (France),
// Max-Planck-Institute Saarbruecken (Germany),
// and Tel-Aviv University (Israel).  All rights reserved. 
//
// This file is part of CGAL (www.cgal.org); you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 3 of the License,
// or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
//
//
// Author(s)     : Andreas Fabri, Stefan Schirra, Sylvain Pion, Michael Hemmer


#ifndef CGAL_GMPZX_TYPE_H
#define CGAL_GMPZX_TYPE_H

#ifdef CGAL_GMPZ_TYPE_H
#error "CGAL_GMPZ_TYPE_H already defined"
#endif

#define CGAL_GMPZ_TYPE_H

#include <CGAL/basic.h>
#include <CGAL/gmp.h>
#include <mpfr.h>

#include <boost/operators.hpp>
#include <CGAL/Handle_for.h>

#include <string>
#include <locale>

#include "spinlock_pool_multi.h"

namespace CGAL {

// TODO : benchmark without ref-counting, and maybe give the possibility
// to select ref-counting or not, then... => template class.

// Wrapper around mpz_t to get the destructor call mpz_clear.
struct Gmpz_rep
{
// FIXME : bug if ~() is called before an mpz_init*() is called.
// not a problem in practice, but not nice.
// maybe the mpz_init_set* functions should move back to Gmpz_rep.
// But then we should use the Storage_traits::construct/get...

  mpz_t mpZ;

  Gmpz_rep() {}
  virtual ~Gmpz_rep() { mpz_clear(mpZ); }

private:
  // Make sure it does not get accidentally copied.
  Gmpz_rep(const Gmpz_rep &);
  Gmpz_rep & operator= (const Gmpz_rep &);
};


class Gmpz
#ifndef CGAL_GMPZ_NO_REFCOUNT
	: Handle_for<Gmpz_rep>,
#else
	: Gmpz_rep,
#endif
    boost::ordered_euclidian_ring_operators1< Gmpz
  , boost::ordered_euclidian_ring_operators2< Gmpz, int
  , boost::ordered_euclidian_ring_operators2< Gmpz, long
  , boost::ordered_euclidian_ring_operators2< Gmpz, unsigned long
  , boost::shiftable< Gmpz , long
  , boost::unit_steppable<Gmpz
  , boost::bitwise<Gmpz
> > > > > > >
{
#ifndef CGAL_GMPZ_NO_REFCOUNT
  typedef Handle_for<Gmpz_rep> Base;
#else
  typedef Gmpz_rep Base;
#endif
public:
  typedef Tag_true  Has_gcd;
  typedef Tag_true  Has_division;
  typedef Tag_true  Has_sqrt;

  typedef Tag_true  Has_exact_ring_operations;
  typedef Tag_true  Has_exact_division;
  typedef Tag_false Has_exact_sqrt;

  Gmpz()
  { mpz_init(mpz()); }

#ifdef CGAL_GMPZ_NO_REFCOUNT
  Gmpz(const Gmpz &z) : Gmpz_rep()
  {
	  Gmp_lock lock(&z);
	  mpz_init_set(mpz(), z.mpz());
  }
  
  Gmpz &operator=(const Gmpz &z)
  {
	  Gmp_lock lock(this, &z);
	  mpz_set(mpz(), z.mpz());
	  return *this;
  }

  void swap(Gmpz &z) {
	  Gmp_lock lock(this, &z);
	  mpz_swap(mpz(), z.mpz());
  }

  bool unique() const {
	  return true;
  }
#endif

  Gmpz(const mpz_t z)
  { mpz_init_set(mpz(), z); }

  Gmpz(int i)
  { mpz_init_set_si(mpz(), i); }

  Gmpz(long l)
  { mpz_init_set_si(mpz(), l); }

  Gmpz(unsigned long l)
  { mpz_init_set_ui(mpz(), l); }

  Gmpz(double d)
  {
     CGAL_warning_msg(is_integer(d), "Gmpz constructed from non-integer double value");
     CGAL_assertion(is_finite(d));
     mpz_init_set_d(mpz(), d);
   }

  Gmpz(const std::string& str, int base = 10)
  { mpz_init_set_str(mpz(), str.c_str(), base); }

  // returns the number of bits used to represent this number
  size_t bit_size() const {
	  Gmp_lock lock(this);
	  return mpz_sizeinbase(mpz(), 2);
  }

  // returns the memory size in bytes
  size_t size() const {
	  Gmp_lock lock(this);
	  return mpz_size(mpz()) / (mp_bits_per_limb / 8);
  }

  // returns the number of decimal digits needed to represent this number
  size_t approximate_decimal_length() const {
	  Gmp_lock lock(this);
	  return mpz_sizeinbase(mpz(), 10);
  }

  double to_double() const {
	  Gmp_lock lock(this);
	  return mpz_get_d(mpz());
  }
  Sign sign() const {
	  Gmp_lock lock(this);
	  return static_cast<Sign>(mpz_sgn(mpz()));
  }

  const mpz_t & mpz() const {
#ifndef CGAL_GMPZ_NO_REFCOUNT
	  return Ptr()->mpZ;
#else
	  return this->mpZ;
#endif
  }
  mpz_t & mpz() {
#ifndef CGAL_GMPZ_NO_REFCOUNT
	  return ptr()->mpZ;
#else
	  return this->mpZ;
#endif
  }

  #ifdef CGAL_ROOT_OF_2_ENABLE_HISTOGRAM_OF_NUMBER_OF_DIGIT_ON_THE_COMPLEX_CONSTRUCTOR
  int tam() const { return 0; }  // put here a code
                                 // measuring the number of digits
                                 // of the Gmpz
#endif

#define CGAL_GMPZ_OBJECT_OPERATOR(_op,_class,_fun)    \
  Gmpz& _op(const _class& z){                        \
    Gmpz Res;                                         \
	Gmp_lock lock(this, &z);                          \
    _fun(Res.mpz(), mpz(), z.mpz());                  \
    swap(Res);                                        \
    return *this;                                     \
  }

  CGAL_GMPZ_OBJECT_OPERATOR(operator+=,Gmpz,mpz_add);
  CGAL_GMPZ_OBJECT_OPERATOR(operator-=,Gmpz,mpz_sub);
  CGAL_GMPZ_OBJECT_OPERATOR(operator*=,Gmpz,mpz_mul);
  CGAL_GMPZ_OBJECT_OPERATOR(operator/=,Gmpz,mpz_tdiv_q);
  CGAL_GMPZ_OBJECT_OPERATOR(operator%=,Gmpz,mpz_tdiv_r);
  CGAL_GMPZ_OBJECT_OPERATOR(operator&=,Gmpz,mpz_and);
  CGAL_GMPZ_OBJECT_OPERATOR(operator|=,Gmpz,mpz_ior);
  CGAL_GMPZ_OBJECT_OPERATOR(operator^=,Gmpz,mpz_xor);
#undef CGAL_GMPZ_OBJECT_OPERATOR

  bool operator<(const Gmpz &b) const
  {
	  Gmp_lock lock(this, &b);
	  return mpz_cmp(this->mpz(), b.mpz()) < 0;
  }
  bool operator==(const Gmpz &b) const
  {
	  Gmp_lock lock(this, &b);
	  return mpz_cmp(this->mpz(), b.mpz()) == 0;
  }


  Gmpz operator+() const {
	  Gmp_lock lock(this);
	  return Gmpz(mpz());
  }
  Gmpz operator-() const {
    Gmpz Res;
	Gmp_lock lock(this);
    mpz_neg(Res.mpz(), mpz());
    return Res;
  }

  Gmpz& operator <<= (const unsigned long& i){
    Gmpz Res;
	Gmp_lock lock(this);
    mpz_mul_2exp(Res.mpz(),this->mpz(), i);
    swap(Res);
    return *this;
  }
  Gmpz& operator >>= (const unsigned long& i){
    Gmpz Res;
	Gmp_lock lock(this);
    mpz_tdiv_q_2exp(Res.mpz(),this->mpz(), i);
    swap(Res);
    return *this;
  }

  Gmpz& operator++() {
	  Gmp_lock lock(this);
	  return *this += 1;
  }
  Gmpz& operator--() {
	  Gmp_lock lock(this);
	  return *this -= 1;
  }


  // interoperability with int
  Gmpz& operator+=(int i);
  Gmpz& operator-=(int i);
  Gmpz& operator*=(int i);
  Gmpz& operator/=(int i);
  bool  operator==(int i) const {
	  Gmp_lock lock(this);
	  return mpz_cmp_si(this->mpz(), i) == 0;
  }
  bool  operator< (int i) const {
	  Gmp_lock lock(this);
	  return mpz_cmp_si(this->mpz(), i) < 0;
  }
  bool  operator> (int i) const {
	  Gmp_lock lock(this);
	  return mpz_cmp_si(this->mpz(), i) > 0;
  }

  // interoperability with long
  Gmpz& operator+=(long i);
  Gmpz& operator-=(long i);
  Gmpz& operator*=(long i);
  Gmpz& operator/=(long i);
  bool  operator==(long i) const {
	  Gmp_lock lock(this);
	  return mpz_cmp_si(this->mpz(), i) == 0;
  }
  bool  operator< (long i) const {
	  Gmp_lock lock(this);
	  return mpz_cmp_si(this->mpz(), i) < 0;
  }
  bool  operator> (long i) const {
	  Gmp_lock lock(this);
	  return mpz_cmp_si(this->mpz(), i) > 0;
  }

  // interoperability with unsigned long
  Gmpz& operator+=(unsigned long i);
  Gmpz& operator-=(unsigned long i);
  Gmpz& operator*=(unsigned long i);
  Gmpz& operator/=(unsigned long i);
  bool  operator==(unsigned long i) const {
	  Gmp_lock lock(this);
	  return mpz_cmp_ui(this->mpz(), i) == 0;
  }
  bool  operator< (unsigned long i) const {
	  Gmp_lock lock(this);
	  return mpz_cmp_ui(this->mpz(), i) < 0;
  }
  bool  operator> (unsigned long i) const {
	  Gmp_lock lock(this);
	  return mpz_cmp_ui(this->mpz(), i) > 0;
  }
};



#define CGAL_GMPZ_SCALAR_OPERATOR(_op,_type,_fun)   \
  inline Gmpz& Gmpz::_op(_type z) {                 \
    Gmpz Res;                                       \
	Gmp_lock lock(this);                            \
    _fun(Res.mpz(), mpz(), z);                      \
    swap(Res);                                      \
    return *this;                                   \
  }

CGAL_GMPZ_SCALAR_OPERATOR(operator*=,int,mpz_mul_si)
CGAL_GMPZ_SCALAR_OPERATOR(operator*=,long,mpz_mul_si)

CGAL_GMPZ_SCALAR_OPERATOR(operator+=,unsigned long,mpz_add_ui)
CGAL_GMPZ_SCALAR_OPERATOR(operator-=,unsigned long,mpz_sub_ui)
CGAL_GMPZ_SCALAR_OPERATOR(operator*=,unsigned long,mpz_mul_ui)
CGAL_GMPZ_SCALAR_OPERATOR(operator/=,unsigned long,mpz_tdiv_q_ui)
#undef CGAL_GMPZ_SCALAR_OPERATOR


inline Gmpz& Gmpz::operator+=(int i)
{
  Gmpz Res;
  Gmp_lock lock(this);
  if (i >= 0)
    mpz_add_ui(Res.mpz(), mpz(), i);
  else
    mpz_sub_ui(Res.mpz(), mpz(), -i);
  swap(Res);
  return *this;
}

inline Gmpz& Gmpz::operator+=(long i)
{
  Gmpz Res;
  Gmp_lock lock(this);
  if (i >= 0)
    mpz_add_ui(Res.mpz(), mpz(), i);
  else
    mpz_sub_ui(Res.mpz(), mpz(), -i);
  swap(Res);
  return *this;
}



inline Gmpz& Gmpz::operator-=(int  i) {
	Gmp_lock lock(this);
	return *this += -i;
}
inline Gmpz& Gmpz::operator-=(long i) {
	Gmp_lock lock(this);
	return *this += -i;
}

inline Gmpz& Gmpz::operator/=(int b) {
  Gmp_lock lock(this);
  if (b>0) {
    Gmpz Res;
    mpz_tdiv_q_ui(Res.mpz(), mpz(), b);
    swap(Res);
    return *this;
  }
  return *this /= Gmpz(b);
}

inline Gmpz& Gmpz::operator/=(long b) {
  Gmp_lock lock(this);
  if (b>0) {
    Gmpz Res;
    mpz_tdiv_q_ui(Res.mpz(), mpz(), b);
    swap(Res);
    return *this;
  }
  return *this /= Gmpz(b);
}

inline
std::ostream&
operator<<(std::ostream& os, const Gmpz &z)
{
  Gmp_lock lock(&z);
  char *str = new char [mpz_sizeinbase(z.mpz(),10) + 2];
  str = mpz_get_str(str, 10, z.mpz());
  os << str ;
  delete[] str;
  return os;
}

inline
void gmpz_eat_white_space(std::istream &is)
{
  std::istream::int_type c;
  do {
    c= is.peek();
    if (c== std::istream::traits_type::eof())
      return;
    else {
      std::istream::char_type cc= c;
      if ( std::isspace(cc, std::locale::classic()) ) {
        is.get();
        // since peek succeeded, this should too
        CGAL_assertion(!is.fail());
      } else {
        return;
      }
    }
  } while (true);
}


inline
std::istream &
gmpz_new_read(std::istream &is, Gmpz &z)
{
  Gmp_lock lock(&z);
  bool negative = false;
  const std::istream::char_type zero = '0';
  std::istream::int_type c;
  Gmpz r;
  std::ios::fmtflags old_flags = is.flags();

  is.unsetf(std::ios::skipws);
  gmpz_eat_white_space(is);

  c=is.peek();
  if (c=='-' || c=='+'){
      is.get();
      CGAL_assertion(!is.fail());
      negative=(c=='-');
      gmpz_eat_white_space(is);
      c=is.peek();
  }

  std::istream::char_type cc= c;

  if (c== std::istream::traits_type::eof() ||
      !std::isdigit(cc, std::locale::classic() ) ){
    is.setstate(std::ios_base::failbit);
  } else {
    CGAL_assertion(cc==c);
    r= cc-zero;
    is.get();
    CGAL_assertion(!is.fail());

    // The following loop was supposed to be an infinite loop with:
    //   while (true)
    // where the break condition is that is.peek() returns and EOF or a
    // non-digit character.
    //
    // Unfortunately, the wording of the C++03 and C++11 standard was not
    // well understood by the authors of libc++ (the STL of clang++) and,
    // in the version of libc++ shipped with Apple-clang-3.2,
    // istream::peek() set the flag eofbit when it reads the last character
    // of the stream *instead* of setting it only when it *tries to read
    // past the last character*. For that reason, to avoid that the next
    // peek() sets also the failbit, one has to check for EOL twice.
    //
    // See the LWG C++ Issue 2036, classified as Not-A-Defect:
    //   http://lwg.github.com/issues/lwg-closed.html#2036
    // and a StackOverflow related question:
    //   http://stackoverflow.com/a/9020292/1728537
    // --
    // Laurent Rineau, 2013/10/10
    while (!is.eof()) {
      c=is.peek();
      if (c== std::istream::traits_type::eof()) {
        break;
      }
      cc=c;
      if  ( !std::isdigit(cc, std::locale::classic() )) {
        break;
      }
      is.get();
      CGAL_assertion(!is.fail());
      CGAL_assertion(cc==c);
      r= r*10+(cc-zero);
    }
  }

  is.flags(old_flags);
  if (!is.fail()) {
    if (negative) {
      z=-r;
    } else {
      z=r;
    }
  }
  return is;
}

/*inline
std::istream&
read_gmpz(std::istream& is, Gmpz &z) {
  bool negative = false;
  bool good = false;
  const int null = '0';
  char c;
  Gmpz tmp;
  std::ios::fmtflags old_flags = is.flags();

  is.unsetf(std::ios::skipws);
  while (is.get(c) && std::isspace(c, std::locale::classic() ))
  {}

  if (c == '-')
  {
        negative = true;
        while (is.get(c) && std::isspace(c, std::locale::classic() ))
        {}
  }
  if (std::isdigit(c, std::locale::classic() ))
  {
        good = true;
        tmp = c - null;
        while (is.get(c) && std::isdigit(c, std::locale::classic() ))
        {
            tmp = 10*tmp + (c-null);
        }
  }
  if (is)
        is.putback(c);
  if (sign(tmp) != ZERO && negative)
      tmp = -tmp;
  if (good){
      z = tmp;
      }
   else
    is.clear(is.rdstate() | std::ios::failbit);

  is.flags(old_flags);
  return is;
  }*/

inline
std::istream&
operator>>(std::istream& is, Gmpz &z)
{
  Gmp_lock lock(&z);
  return gmpz_new_read(is, z);
}

template <>
struct Split_double<Gmpz>
{
  void operator()(double d, Gmpz &num, Gmpz &den) const
  {
    std::pair<double, double> p = split_numerator_denominator(d);
	Gmp_lock lock(&num, &den);
    num = Gmpz(p.first);
    den = Gmpz(p.second);
  }
};

inline Gmpz min BOOST_PREVENT_MACRO_SUBSTITUTION(const Gmpz& x,const Gmpz& y){
  return (x<y)?x:y;
}
inline Gmpz max BOOST_PREVENT_MACRO_SUBSTITUTION(const Gmpz& x,const Gmpz& y){
  return (x<y)?y:x;
}


} //namespace CGAL

#endif // CGAL_GMPZ_TYPE_H
