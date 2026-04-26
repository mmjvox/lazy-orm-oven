#include "IDbConnection.h"

namespace LazyOrm {

IDbConnection::IDbConnection() {}

IDbConnection::~IDbConnection() {}

std::string IDbConnection::getLastError() const {
    return mLastError;
}

}
