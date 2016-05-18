/*!
 * This file contains simple custom objects for exception
 * handling in the lowtis library.
 *
 * \author Stephen Plaza (plazas@janelia.hhmi.org)
*/

#ifndef LOWTISEXCEPTION_H
#define LOWTISEXCEPTION_H

#include <sstream>

namespace lowtis {

/*!
 * Exception handling for general lowtis errors.
 * It is a wrapper for simple string message.
*/
class LowtisErr : public std::exception { 
  public:
    /*!
     * Construct takes a string message for the error.
     * \param msg_ string message
    */
    explicit LowtisErr(std::string msg_) : msg(msg_) {}
    
    /*!
     * Implement exception base class function.
    */
    virtual const char* what() const throw()
    {
        return msg.c_str();
    }

    /*!
     * Empty destructor.
    */
    virtual ~LowtisErr() throw() {}
  protected:
    
    //! Error message
    std::string msg;
};

/*!
 * Function that allows formatting of error to standard output.
*/
std::ostream& operator<<(std::ostream& os, LowtisErr& err);

}

#endif
