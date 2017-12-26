#define _USE_MATH_DEFINES
#define WIN_TITLE  "Congratulations on your wedding! Heng Tuo!"
#define WIN_WIDTH  800 
#define WIN_HEIGHT 600
#define IMAGE_DIR  "images\\"

#include <vector>
#include <algorithm>
#include <iterator>
#include <string>
#include <locale>
#include <codecvt>

#include "toaster/PixelToaster.h"
#include "imgui/imgui.h"
#include "Cimg/CImg.h"
#include "dirent.h"
#include "drawing.h"
#include "imgui/imgui_internal.h"

using namespace std;
using namespace PixelToaster;
using namespace cimg_library;


static float start_time = ImGui::GetTime();

struct toaster_framebuffer_t final : framebuffer_t
{
    typedef PixelToaster::TrueColorPixel pixel_t;

    PixelToaster::Display& display;
    std::vector<pixel_t>   colors;

    toaster_framebuffer_t(PixelToaster::Display& display) :
        framebuffer_t(display.width(), display.height()),
        display(display),
        colors(width * height, pixel_t(0, 0, 0, 0))
    {
    }

    void fill_rect_2d(int x0, int y0, int x1, int y1, float color) override final
    {
        auto pixel = color_to_pixel(color);

        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);

        auto out_row = colors.data() + x0 + y0 * width;
        for (int y = y0; y < y1; ++y, out_row += width)
        {
            auto out = out_row;
            for (int x = x0; x < x1; ++x, ++out)
                *out = pixel;
        }
    }

    virtual void char_2d(const font_t& font, int x, int y, char c, float color) override final
    {
        auto data = font.find(c);
        if (!data)
            return;

        typedef bool(*unpack_font_proc)(const uint8_t* data, int x, int y);

        unpack_font_proc unpack_font = nullptr;
        switch (font.pack)
        {
        case font_pack_row_low:     unpack_font = [](const uint8_t* data, int x, int y) { return (data[x] & (1 << y)) != 0; }; break;
        case font_pack_row_high:    unpack_font = [](const uint8_t* data, int x, int y) { return (data[x] & (1 << (7 - y))) != 0; }; break;
        case font_pack_column_low:  unpack_font = [](const uint8_t* data, int x, int y) { return (data[y] & (1 << x)) != 0; }; break;
        case font_pack_column_high: unpack_font = [](const uint8_t* data, int x, int y) { return (data[y] & (1 << (7 - x))) != 0; }; break;
        default: return;
        }

        auto color_pixel = color_to_pixel(color);
        auto bg_pixel = color_to_pixel(0);

        auto out_row = colors.data() + x + y * width;
        for (int y = 0; y < font.h; ++y, out_row += width)
        {
            auto out = out_row;
            for (int x = 0; x < font.w; ++x, ++out)
                *out = unpack_font(data, x, y) ? color_pixel : bg_pixel;
        }
    }

protected:
    virtual void clear_color(float c) override final
    {
        colors.assign(width * height, color_to_pixel(c));
    }

    virtual void set_color(int x, int y, float c) override final
    {
        auto back = colors[x + width * y];
        auto pixel = color_to_pixel(c);

        colors[x + width * y] = pixel;
    }

    virtual void blend_color(int x, int y, float c, float a) override final
    {
        auto back = colors[x + width * y];
        auto pixel = color_to_pixel(c);

        auto ia = (int)(a * 255);

        pixel.r = (int)back.r + ((int)pixel.r - (int)back.r) * ia / 255;
        pixel.g = (int)back.g + ((int)pixel.g - (int)back.g) * ia / 255;
        pixel.b = (int)back.b + ((int)pixel.b - (int)back.b) * ia / 255;

        colors[x + y * width] = pixel;
    }

    virtual void commit_impl() override final
    {
    }

    virtual void present_impl() override final
    {
        display.update(colors);
    }

private:
    pixel_t color_to_pixel(float c) const
    {
        auto brightness = std::max(0, std::min(255, (int)(255 * c)));
        return pixel_t(brightness, brightness, brightness, 255);
    }
};

static framebuffer_t* imgui_render_target = nullptr;
static void render_draw_lists(ImDrawData* draw_data)
{
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* const cmd_list = draw_data->CmdLists[n];

        auto& vertices = cmd_list->VtxBuffer;
        auto& indices = cmd_list->IdxBuffer;

        int idx_offset = 0;
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else if (pcmd->ElemCount > 0)
            {
                auto* const indexStart = indices.Data + idx_offset;

                for (unsigned int i = 0; i < pcmd->ElemCount; i += 3)
                {
                    const auto i0 = indexStart[i + 0];
                    const auto i1 = indexStart[i + 1];
                    const auto i2 = indexStart[i + 2];

                    const auto v0 = vertices[i0];
                    const auto v1 = vertices[i1];
                    const auto v2 = vertices[i2];

                    const auto vc0 = ImColor(v0.col);
                    const auto vc1 = ImColor(v1.col);
                    const auto vc2 = ImColor(v2.col);

                    const auto c0 = (vc0.Value.x + vc0.Value.y + vc0.Value.z) / 3;
                    const auto c1 = (vc1.Value.x + vc1.Value.y + vc1.Value.z) / 3;
                    const auto c2 = (vc2.Value.x + vc2.Value.y + vc2.Value.z) / 3;

                    if (pcmd->TextureId)
                    {
                        auto& image = *reinterpret_cast<image_t*>(pcmd->TextureId);

                        generic_triangle_2d(*imgui_render_target, image,
                            v0.pos.x, v0.pos.y,
                            v2.pos.x, v2.pos.y,
                            v1.pos.x, v1.pos.y,
                            v0.uv.x, v0.uv.y,
                            v2.uv.x, v2.uv.y,
                            v1.uv.x, v1.uv.y,
                            c0, c2, c1,
                            vc0.Value.w, vc2.Value.w, vc1.Value.w);
                    }
                    else
                    {
                        generic_triangle_2d(*imgui_render_target,
                            static_cast<int>(v0.pos.x), static_cast<int>(v0.pos.y),
                            static_cast<int>(v2.pos.x), static_cast<int>(v2.pos.y),
                            static_cast<int>(v1.pos.x), static_cast<int>(v1.pos.y),
                            c0);
                    }
                }

                //fill_rect_2d(*imgui_render_target, (int)min.x, (int)min.y, (int)max.x, (int)max.y, 1.0f);

                //const D3D10_RECT r = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };
                //ctx->PSSetShaderResources(0, 1, (ID3D10ShaderResourceView**)&pcmd->TextureId);
                //ctx->RSSetScissorRects(1, &r);
                //ctx->DrawIndexed(pcmd->ElemCount, idx_offset, vtx_offset);
            }
            idx_offset += pcmd->ElemCount;
        }
    }
}

struct imgui_listener final : public PixelToaster::Listener
{
    virtual void onMouseButtonDown(PixelToaster::DisplayInterface& display, PixelToaster::Mouse mouse) override final
    {
        auto& io = ImGui::GetIO();
        io.MousePos.x = mouse.x * imgui_render_target->width / display.width();
        io.MousePos.y = mouse.y * imgui_render_target->height / display.height();
        if (mouse.buttons.left)   io.MouseDown[0] = true;
        if (mouse.buttons.right)  io.MouseDown[1] = true;
        if (mouse.buttons.middle) io.MouseDown[2] = true;
    }

    virtual void onMouseButtonUp(PixelToaster::DisplayInterface& display, PixelToaster::Mouse mouse) override final
    {
        auto& io = ImGui::GetIO();
        io.MousePos.x = mouse.x * imgui_render_target->width / display.width();
        io.MousePos.y = mouse.y * imgui_render_target->height / display.height();
        if (io.MouseDown[0] && !mouse.buttons.left)   io.MouseDown[0] = false;
        if (io.MouseDown[1] && !mouse.buttons.right)  io.MouseDown[1] = false;
        if (io.MouseDown[2] && !mouse.buttons.middle) io.MouseDown[2] = false;
    }

    virtual void onMouseMove(PixelToaster::DisplayInterface& display, PixelToaster::Mouse mouse) override final
    {
        auto& io = ImGui::GetIO();
        io.MousePos.x = mouse.x * imgui_render_target->width / display.width();
        io.MousePos.y = mouse.y * imgui_render_target->height / display.height();
    }

    /// On key down.
    /// Called once only when a key is pressed and held.
    /// @param display the display sending the event
    /// @param key the key event data.
    virtual void onKeyDown(DisplayInterface & display, Key key) override final
    {
        auto&io = ImGui::GetIO();
        io.KeysDown[key] = true;
    }

    /// On key pressed.
    /// Called multiple times while a key is pressed and held including the initial event.
    /// @param display the display sending the event
    /// @param key the key event data.

    virtual void onKeyPressed(DisplayInterface & display, Key key) override final
    {
        auto&io = ImGui::GetIO();
        io.KeysDown[key] = true;
    }


    /// On key up.
    /// Called when a key is released.
    /// @param display the display sending the event
    /// @param key the key event data.

    virtual void onKeyUp(DisplayInterface & display, Key key) override final {
        auto&io = ImGui::GetIO();
        io.KeysDown[key] = false;
    }
};

float gen_rd(const int x, const int y)
{
    double a = 0, b = 0, c, d, n = 0;
    while ((c = a*a) + (d = b*b) < 4 && n++ < 880)
    {
        b = 2 * a*b + y*8e-9 - .645411; a = c - d + x*8e-9 + .356888;
    }
    return pow((n - 80) / 800, 3.);
}


float gen_gr(const int x, const int y)
{
    double a = 0, b = 0, c, d, n = 0;
    while ((c = a*a) + (d = b*b) < 4 && n++ < 880)
    {
        b = 2 * a*b + y*8e-9 - .645411; a = c - d + x*8e-9 + .356888;
    }
    return pow((n - 80) / 800, .7);

}

float gen_bl(const int x, const int y)
{
    double a = 0, b = 0, c, d, n = 0;
    while ((c = a*a) + (d = b*b) < 4 && n++ < 880)
    {
        b = 2 * a*b + y*8e-9 - .645411; a = c - d + x*8e-9 + .356888;
    }
    return pow((n - 80) / 800, .5);
}


void read_images(vector<string>& images)
{
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(".\\images")) != nullptr) {
        /* print all the files and directories within directory */
        while ((ent = readdir(dir)) != nullptr)
        {
            string path = ent->d_name;
            const auto pos = path.find_first_of('.');

            const auto post_fix = path.substr(pos + 1, path.size() - pos - 1);
            if (post_fix == "jpg" || post_fix == "jpeg" || post_fix == "bmp" || post_fix == "png")
                images.push_back(ent->d_name);
        }
        closedir(dir);
    }
    else {
        /* could not open directory */
        perror("");
    }
}


// Usage:
//  static ExampleAppLog my_log;
//  my_log.AddLog("Hello %d world\n", 123);
//  my_log.Draw("title");
struct ExampleAppLog
{
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets;        // Index to lines offset
    bool                ScrollToBottom;

    void    Clear() { Buf.clear(); LineOffsets.clear(); }

    void    AddLog(const char* fmt, ...) IM_PRINTFARGS(2)
    {
        int old_size = Buf.size();
        va_list args;
        va_start(args, fmt);
        Buf.appendv(fmt, args);
        va_end(args);
        for (int new_size = Buf.size(); old_size < new_size; old_size++)
            if (Buf[old_size] == '\n')
                LineOffsets.push_back(old_size);
        ScrollToBottom = true;
    }

    void    Draw(const char* title, bool* p_open = NULL)
    {
        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiSetCond_FirstUseEver);
        ImGui::Begin(title, p_open);
        if (ImGui::Button("Clear")) Clear();
        ImGui::SameLine();
        bool copy = ImGui::Button("Copy");
        ImGui::SameLine();
        Filter.Draw("Filter", -100.0f);
        ImGui::Separator();
        ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        if (copy) ImGui::LogToClipboard();

        if (Filter.IsActive())
        {
            const char* buf_begin = Buf.begin();
            const char* line = buf_begin;
            for (int line_no = 0; line != NULL; line_no++)
            {
                const char* line_end = (line_no < LineOffsets.Size) ? buf_begin + LineOffsets[line_no] : NULL;
                if (Filter.PassFilter(line, line_end))
                    ImGui::TextUnformatted(line, line_end);
                line = line_end && line_end[1] ? line_end + 1 : NULL;
            }
        }
        else
        {
            ImGui::TextUnformatted(Buf.begin());
        }

        if (ScrollToBottom)
            ImGui::SetScrollHere(1.0f);
        ScrollToBottom = false;
        ImGui::EndChild();
        ImGui::End();
    }
};

void
do_debug(bool* pause)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    if (ImGui::Begin("Debug:", nullptr, ImVec2(0, 0), 0.0f, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    {
        //ImGui::Text(u8"average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        //ImGui::Text(u8"Mouse Position: (%.1f,%.1f)", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
        //ImGui::Spacing();

        //ImGui::Checkbox("Pause", pause);
    }
}

void
do_exhibition(vector<CImg<float>>& images, toaster_framebuffer_t& buffer)
{
    auto image = images[static_cast<int>(ImGui::GetTime() - start_time) % images.size()];

    unsigned int index = 0;
    const auto x_iv = max(0, (WIN_WIDTH - image.width()) / 2);
    const auto y_iv = max(0, (WIN_HEIGHT - image.height()) / 2);
    for (auto y = 0; y < WIN_HEIGHT; ++y)
    {
        for (auto x = 0; x < WIN_WIDTH; ++x)
        {
            if (x < image.width() && y < image.height()) {
                buffer.colors[x + x_iv + (y + y_iv)*WIN_WIDTH].r = static_cast<float>(*image.data(x, y, 0, 0));
                buffer.colors[x + x_iv + (y + y_iv)*WIN_WIDTH].g = static_cast<float>(*image.data(x, y, 0, 1));
                buffer.colors[x + x_iv + (y + y_iv)*WIN_WIDTH].b = static_cast<float>(*image.data(x, y, 0, 2));
            }
            ++index;
        }
    }
}

string
convert_wchar_to_char(const wstring& string_to_convert)
{
    //setup converter
    //wstring_convert<codecvt_utf8<wchar_t>, wchar_t> converter;
    wstring_convert<codecvt_utf8<wchar_t>> converter;

    //use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
    return converter.to_bytes(string_to_convert);
}


void do_greet(bool greet_open, const int rate, int greet_status[4], wstring greet_words[4], ImGuiIO& io)
{
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.2, io.DisplaySize.y * 0.2));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.6, io.DisplaySize.y * 0.6), ImGuiSetCond_FirstUseEver);

    if (ImGui::Begin("Greetings", &greet_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar))
    {
        for (auto idx = 0; idx < 4; idx++)
        {
            if (greet_status[idx] == 0 && (idx == 0 || greet_status[idx - 1]) || greet_status[idx])
            {
                wstring words = greet_words[idx];
                wstring text;

                if (greet_status[idx])
                {
                    text = words;
                }
                else
                {
                    const auto len = static_cast<int>((ImGui::GetTime() - start_time) * rate);
                    text = words.substr(0, len);
                    // output will loss the first some words when convert here, no ideas about it
                    // text = convert_wchar_to_char(words.substr(0, len)).c_str();
                }

                ImGui::BeginChild(idx + 'a', ImVec2(20, 300), false, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextWrapped(convert_wchar_to_char(text).c_str());
                ImGui::EndChild();
                ImGui::SameLine();

                if (!greet_status[idx] && (ImGui::GetTime() - start_time) * rate > words.size())
                {
                    start_time = ImGui::GetTime();
                    greet_status[idx] = 1;
                }

                if (greet_status[3]) { greet_open = false; }
            }
        }
    }
}

int wmain()
{
    Display display(WIN_TITLE, WIN_WIDTH, WIN_HEIGHT);

    image_t font_atlas = {};
    {
        auto& io = ImGui::GetIO();
        io.DisplaySize.x = WIN_WIDTH;
        io.DisplaySize.y = WIN_HEIGHT;
        io.RenderDrawListsFn = render_draw_lists;

        unsigned char* pixels;
        int width, height;

        ImFontConfig config;
        config.MergeMode = true;
        config.OversampleH = config.OversampleV = 1;
        config.PixelSnapH = true;

        io.Fonts->AddFontDefault();
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyhbd.ttc", 18, &config, io.Fonts->GetGlyphRangesChinese());
        io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

        font_atlas.data = pixels;
        font_atlas.width = width;
        font_atlas.height = height;
        font_atlas.pitch = width;

        io.Fonts->TexID = &font_atlas;
    }

    // why not satisfied when use palihore;
    imgui_listener listener;
    display.listener(&listener);

    // logger
    static ExampleAppLog logger;
    bool debug_open = false;
    bool greet_open = true;

    auto pause = false;
    auto time = 0.00f;

    Timer timer;
    timer.reset();

    // greetings
    const auto rate = 2;
    int greet_status[4] = { 0 };
    wstring greet_words[4] = {
        L"两姓联姻，一堂缔约，良缘永结，匹配同称。",
        L"看此日桃花灼灼，宜室宜家，卜他年瓜瓞绵绵，尔昌尔炽。",
        L"现谨以白头之约，书向鸿笺，好将红叶之盟，载明鸳谱。",
        L"祝哥哥嫂嫂白头偕老，永结同心。"
    };

    vector<string> image_paths;
    read_images(image_paths);

    // input img
    vector<CImg<float>> images;
    for (string img_path : image_paths)
    {
        img_path = static_cast<string>(IMAGE_DIR) + img_path;
        const CImg<float> img(img_path.c_str());
        images.push_back(img);
    }

    // base buffer
    const toaster_framebuffer_t display_buffer(display);
    while (display.open())
    {
        auto buffer = display_buffer;
        imgui_render_target = &buffer;

        auto& io = ImGui::GetIO();
        io.DisplaySize.x = static_cast<float>(imgui_render_target->width);
        io.DisplaySize.y = static_cast<float>(imgui_render_target->height);
        io.DeltaTime = static_cast<float>(timer.delta());

        if (!pause)
            time += io.DeltaTime;

        ImGui::NewFrame();
        buffer.clear(0, 1.0f);

        if (ImGui::IsKeyPressed(122)) debug_open = !debug_open;
        if (debug_open) do_debug(&pause);
        if (greet_open) do_greet(greet_open, rate, greet_status, greet_words, io);
        if (!greet_open) do_exhibition(images, buffer);
        if (debug_open) logger.Draw("Debug: logger", &debug_open);

        ImGui::End();
        if (ImGui::GetCurrentWindowRead())
            ImGui::Render();
        display.update(buffer.colors);
    }
}


