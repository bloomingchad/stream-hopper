#include "UI/Layout/FullLayoutStrategy.h"
#include "UI/HeaderBar.h"
#include "UI/FooterBar.h"
#include "UI/StationsPanel.h"
#include "UI/NowPlayingPanel.h"
#include "UI/HistoryPanel.h"
#include "UI/StateSnapshot.h"

void FullLayoutStrategy::calculateDimensions(
    int width, int height,
    HeaderBar& header, FooterBar& footer,
    StationsPanel& stations, NowPlayingPanel& now_playing, HistoryPanel& history,
    const StateSnapshot& snapshot
) {
    int content_h = height - 2; // -2 for header/footer
    int left_panel_w = std::max(35, width / 3);
    int right_panel_w = width - left_panel_w;
    int top_right_h = snapshot.is_auto_hop_mode_active ? 7 : 6;
    int bottom_right_h = content_h - top_right_h;

    header.setDimensions(0, 0, width, 1);
    footer.setDimensions(height - 1, 0, width, 1);
    stations.setDimensions(1, 0, left_panel_w, content_h);
    now_playing.setDimensions(1, left_panel_w, right_panel_w, top_right_h);
    history.setDimensions(1 + top_right_h, left_panel_w, right_panel_w, bottom_right_h);
}
