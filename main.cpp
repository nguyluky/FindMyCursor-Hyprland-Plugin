#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/EventManager.hpp>
#include <hyprland/src/Compositor.hpp>

HANDLE PHANDLE = nullptr;

static wl_event_source *tick = nullptr;
static bool isZoomed = false;


APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}


void setCurorSize(int size)
{
    std::string theme = getenv("XCURSOR_THEME");
    Debug::log(LOG, "Set cursor theme: " + theme);
    HyprlandAPI::invokeHyprctlCommand("setcursor", theme + " " + std::to_string(size));
    std::string command2 = "gsettings set org.gnome.desktop.interface cursor-size " + std::to_string(size);
    system(command2.c_str());
}

void onMouseMove(const Vector2D &pos)
{
    static std::vector<std::pair<Vector2D, std::chrono::steady_clock::time_point>> history;

    // Thêm vị trí và thời gian hiện tại vào lịch sử
    auto currentTime = std::chrono::steady_clock::now();
    history.emplace_back(pos, currentTime);

    // Xóa các điểm cũ hơn 1 giây
    while (!history.empty() &&
           std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - history.front().second).count() > 500)
    {
        history.erase(history.begin());
    }

    // Ngưỡng phát hiện lắc
    const double DIST_THRESHOLD = 10.0;  // Khoảng cách tối thiểu mỗi chuyển động (pixel)
    const double RANGE_THRESHOLD = 200.0; // Phạm vi di chuyển tối đa (pixel)
    const int MIN_CHANGES = 4;           // Số lần đổi hướng tối thiểu
    const int MIN_POINTS = 5;            // Số điểm tối thiểu để phân tích

    // Kiểm tra lắc
    bool isShaking = false;
    if (history.size() >= MIN_POINTS)
    {
        int directionChanges = 0;
        double lastDx = 0.0;
        double minX = history[0].first.x, maxX = history[0].first.x;
        double minY = history[0].first.y, maxY = history[0].first.y;

        for (size_t i = 1; i < history.size(); ++i)
        {
            const auto &prev = history[i - 1].first;
            const auto &curr = history[i].first;

            // Tính khoảng cách
            double distance = std::sqrt(std::pow(curr.x - prev.x, 2) + std::pow(curr.y - prev.y, 2));
            if (distance > DIST_THRESHOLD)
            {
                // Cập nhật phạm vi
                minX = std::min(minX, curr.x);
                maxX = std::max(maxX, curr.x);
                minY = std::min(minY, curr.y);
                maxY = std::max(maxY, curr.y);

                // Kiểm tra đổi hướng (dựa trên x)
                double dx = curr.x - prev.x;
                if (i > 1 && dx * lastDx < 0)
                {
                    directionChanges++;
                }
                lastDx = dx;
            }
        }

        double rangeX = maxX - minX;
        double rangeY = maxY - minY;


        // log
        // Debug::log(LOG, "RangeX: {}, RangeY: {}, Direction Changes: {}", rangeX, rangeY, directionChanges);

        // Kiểm tra điều kiện lắc
        if (directionChanges >= MIN_CHANGES)
        {
            isShaking = true;
        }
    }

    // Xử lý lắc
    if (isShaking && !isZoomed)
    {
        isZoomed = true;
        Debug::log(LOG, "Set cursor size to 64");
        setCurorSize(64);
        wl_event_source_timer_update(tick, 2000);
    }
}

int onTick(void *data)
{
    // Xử lý sự kiện tick ở đây
    // Ví dụ: kiểm tra trạng thái của con trỏ chuột hoặc thực hiện các hành động khác
    Debug::log(LOG, "Tick event triggered");
    setCurorSize(24);
    isZoomed = false;
    return 1;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH)
    {
        HyprlandAPI::addNotification(PHANDLE, "[Test] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hww] Version mismatch");
    }

    static auto m_pMouseMoveCallback = HyprlandAPI::registerCallbackDynamic( //
        PHANDLE, "mouseMove", [&](void *self, SCallbackInfo &info, std::any param)
        { onMouseMove(std::any_cast<Vector2D>(param)); });
    
        tick = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, &onTick, nullptr);

    HyprlandAPI::addNotification(PHANDLE, "[ShakeToZoom] Initialized successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"ShakeToZoom", "A cursor zoom plugin that enlarges the cursor when you shake it", "Ng", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT()
{
    // Cleanup code here
}
