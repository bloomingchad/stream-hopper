#ifndef MPVINSTANCE_H
#define MPVINSTANCE_H

#include <string>
#include <mpv/client.h>

class MpvInstance {
public:
    MpvInstance();
    ~MpvInstance();

    // Delete copy operations to prevent accidental duplication of the handle
    MpvInstance(const MpvInstance&) = delete;
    MpvInstance& operator=(const MpvInstance&) = delete;

    // Allow move operations
    MpvInstance(MpvInstance&& other) noexcept;
    MpvInstance& operator=(MpvInstance&& other) noexcept;

    void initialize(const std::string& url);
    mpv_handle* get() const;

private:
    mpv_handle* m_mpv;
};

#endif // MPVINSTANCE_H
