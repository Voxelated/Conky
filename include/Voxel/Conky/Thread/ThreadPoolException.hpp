//==--- Conky/Thread/ThreadPoolException.hpp --------------- -*- C++ -*- ---==//
//            
//                                Voxel : Conky
//
//                        Copyright (c) 2017 Rob Clucas
//  
//  This file is distributed under the MIT License. See LICENSE for details.
//
//==------------------------------------------------------------------------==//
//
/// \file  ThreadPoolException.hpp
/// \brief This file provides the definition of an exception class for thread
///        pool related exceptions.
//
//==------------------------------------------------------------------------==//

#ifndef VOXX_CONKY_THREAD_THREAD_POOL_EXCEPTION_HPP
#define VOXX_CONKY_THREAD_THREAD_POOL_EXCEPTION_HPP

#include <exception>
#include <string>

namespace Voxx::Conky {

/// The ThreadPoolException class overrides the standard exception class to
/// define thread pool related exceptions.
class ThreadPoolException : public std::exception {
 public:
  /// The Type enum defines the types of thread pool realted exceptions.
  enum class Type : uint8_t {
    Oversubscription = 0x00  //!< More threads than cores.
  };

 public:
  /// Constructor -- initializes the type of the thread error.
  /// \param[in]  type          The type of the thread error.
  /// \param[in]  threadNumber  The number of the thread with the error.
  ThreadPoolException(Type type) noexcept : ErrorType(type) {}

  /// Display a message for the exception.
  virtual const char* what() const noexcept override {
    const std::string msg = std::string("\nThreadPool Exception:\n\t")
                          + message();
    return msg.c_str();
  }

 private:
  Type ErrorType; //!< The type of the thread pool exception.
                  
  /// Returns char string pointer with an appropriate message for the type of
  /// the error.
  const char* message() const noexcept;
};

} // namespace Voxx::Conky

#endif // VOXX_CONKY_THREAD_THREAD_POOL_EXCEPTION_HPP