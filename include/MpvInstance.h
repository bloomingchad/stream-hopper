#ifndef MPVINSTANCE_H
#define MPVINSTANCE_H

#include <string>
#include <mpv/client.h>

class MpvInstance {
public:
    MpvInstance();
    ~MpvInstance();

    MpvInstance(const MpvInstance&) = delete;
    MpvInstance& operator=(const MpvInstance&) = delete;
    MpvInstance(MpvInstance&& other) noexcept;
    MpvInstance& operator=(MpvInstance&& other) noexcept;

    void initialize(const std::string& url);
    void shutdown(); // New method for explicit shutdown
    mpv_handle* get() const;

private:
    mpv_handle* m_mpv;
};

#endif // MPVINSTANCE_H
