#pragma once

#include <exception>
#include <string>
class DisconnectException : public std::exception
{
public:
    DisconnectException(const std::string& msg) : m_msg(msg)
    {
    }


   ~DisconnectException() _NOEXCEPT
   {

   }

   virtual const char* what() const throw ()
   {
        return m_msg.c_str();
   }

   const std::string m_msg;
};
