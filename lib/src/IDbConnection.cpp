#include "IDbConnection.h"

IDbConnection::IDbConnection() {}

IDbConnection::~IDbConnection() {}

std::string IDbConnection::getLastError() const {
    return mLastError;
}
