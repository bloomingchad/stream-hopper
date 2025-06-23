#include "UI/Layout/CompactLayoutStrategy.h"

#include "UI/FooterBar.h"
#include "UI/HeaderBar.h"
#include "UI/HistoryPanel.h"
#include "UI/NowPlayingPanel.h"
#include "UI/StateSnapshot.h"
#include "UI/StationsPanel.h"

void CompactLayoutStrategy::calculateDimensions(int width,
                                                int height,
                                                HeaderBar& header,
                                                FooterBar& footer,
                                                StationsPanel& stations,
                                                NowPlayingPanel& now_playing,
                                                HistoryPanel& history,
                                                const StateSnapshot& snapshot) {
    int now_playing_h = snapshot.is_auto_hop_mode_active ? 6 : 5;
    int content_h = height - 2; // -2 for header/footer
    int remaining_h = content_h - now_playing_h;
    int stations_h = std::max(3, static_cast<int>(remaining_h * 0.6));
    int history_h = remaining_h - stations_h;

    if (history_h < 3) {
        stations_h = remaining_h;
        history_h = 0;
    }

    int now_playing_y = 1;
    int stations_y = now_playing_y + now_playing_h;
    int history_y = stations_y + stations_h;

    header.setDimensions(0, 0, width, 1);
    footer.setDimensions(height - 1, 0, width, 1);
    now_playing.setDimensions(now_playing_y, 0, width, now_playing_h);
    stations.setDimensions(stations_y, 0, width, stations_h);

    if (history_h > 0) {
        history.setDimensions(history_y, 0, width, history_h);
    } else {
        history.setDimensions(0, 0, 0, 0); // Effectively hide it
    }
}
