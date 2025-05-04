#include <string>
#include "File.h"
#include "SingleFileStorage.h"

class WalFile
{
    File file;
    int64_t offset = 0;
    size_t _items = 0;

    bool needsWrite(const SingleFileStorage::SFragInfo& info);

public:
    WalFile(const std::string &path);

    bool write(const int64_t transid, const SingleFileStorage::SFragInfo& info);
    std::vector<SingleFileStorage::SFragInfo> read(int64_t transid);
    void reset();
    bool sync();

    size_t items() const
    {
        return _items;
    }
};