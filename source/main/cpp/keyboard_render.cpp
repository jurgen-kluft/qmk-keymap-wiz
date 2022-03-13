#include "xbase/x_base.h"
#include "xbase/x_context.h"
#include "xbase/x_memory.h"
#include "qmk-keymap-wiz/keyboard_data.h"

#include "libimgui/imgui.h"
#include "libimgui/imgui_internal.h"
#include "libimgui/imgui_impl_glfw.h"
#include "libimgui/imgui_impl_opengl3.h"

#include <stdio.h>
#include <math.h> // sqrtf, powf, cosf, sinf, floorf, ceilf


static ImVec4 Darken(ImVec4 const& c, float p)
{
    ImVec4 r;
    r.x = c.x - (c.x * p);
    r.y = c.y - (c.y * p);
    r.z = c.z - (c.z * p);
    r.w = c.w;
    return r;
}

static ImVec4 DarkenAlpha(ImVec4 const& c, float p, float a)
{
    ImVec4 r;
    r.x = c.x - (c.x * p);
    r.y = c.y - (c.y * p);
    r.z = c.z - (c.z * p);
    r.w = c.w - (c.z * a);
    return r;
}

static ImVec4 Lighten(ImVec4 const& c, float p)
{
    ImVec4 r;
    r.x = c.x + (c.x * p);
    r.y = c.y + (c.y * p);
    r.z = c.z + (c.z * p);

    float max = c.x;
    if (c.y > max)
        max = c.y;
    if (c.z > max)
        max = c.z;

    if (max > 1.0f)
    {
        r.x /= max;
        r.y /= max;
        r.z /= max;
    }

    r.w = c.w;
    return r;
}

ImVec2 operator-(const ImVec2& l, const ImVec2& r) { return {l.x - r.x, l.y - r.y}; }
struct ImRotation
{
    ImRotation(ImDrawList* draw_list)
    {
        m_draw_list = draw_list;
        m_start     = draw_list->VtxBuffer.Size;
    }

    ImVec2 Center()
    {
        ImVec2 l(FLT_MAX, FLT_MAX), u(-FLT_MAX, -FLT_MAX); // bounds

        const auto& buf = m_draw_list->VtxBuffer;
        for (int i = m_start; i < buf.Size; i++)
            l = ImMin(l, buf[i].pos), u = ImMax(u, buf[i].pos);

        return ImVec2((l.x + u.x) / 2, (l.y + u.y) / 2); // or use _ClipRectStack?
    }

    void Apply(float rad)
    {
        ImVec2 center = Center();

        float s = (float)sin(rad), c = (float)cos(rad);
        center = ImRotate(center, c, s) - center;

        auto& buf = m_draw_list->VtxBuffer;
        for (int i = m_start; i < buf.Size; i++)
            buf[i].pos = ImRotate(buf[i].pos, c, s) - center;
    }

    ImDrawList* m_draw_list;
    int         m_start;
};

static bool IsPointInsideKey(xcore::keyboard_resource_t const* kb, xcore::keygroup_resource_t const& kg, xcore::key_resource_t const& key, float globalscale, float x, float y, float px, float py)
{
    const float sw = key.m_sw * kg.m_sw * kb->m_sw * kb->m_scale * globalscale;
    const float sh = key.m_sh * kg.m_sh * kb->m_sh * kb->m_scale * globalscale;
    const float kw = (key.m_w * kg.m_w * kb->m_w * kb->m_scale * globalscale) - (2 * sw);
    const float kh = (key.m_h * kg.m_h * kb->m_h * kb->m_scale * globalscale) - (2 * sh);
    const float hw = kw / 2;
    const float hh = kh / 2;
    if (px >= (x - hw) && px <= (x + hw) && py >= (y - hh) && py <= (y + hh))
        return true;
    return false;
}

ImFont* KbFonts[] = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

static ImVec2 SelectFontSize(const char* txt, float kw)
{
    ImGui::PushFont(KbFonts[0]);
    ImVec2 td = ImGui::CalcTextSize(txt);
    if ((td.x * 1.1f) >= kw)
    {
        ImGui::PopFont();

        ImGui::PushFont(KbFonts[1]);
        td = ImGui::CalcTextSize(txt);
        if ((td.x * 1.1f) >= kw)
        {
            ImGui::PopFont();

            ImGui::PushFont(KbFonts[2]);
            td = ImGui::CalcTextSize(txt);
            if ((td.x * 1.1f) >= kw)
            {
                ImGui::PopFont();

                ImGui::PushFont(KbFonts[3]);
                td = ImGui::CalcTextSize(txt);
            }
        }
    }
    return td;
}

void keyboard_loadfonts()
{
    static const ImWchar icon_ranges[] = {
        0xf000,
        0xf3ff,
        0,
    };
    static const ImWchar symbol_ranges[] = {
        0x2300,
        0x23ff,
        0,
    };
    static const ImWchar base_ranges[] = {
        0x0020,
        0x00ff,
        0,
    };

    ImFontConfig config;
    config.MergeMode = true;

    ImGuiIO& io = ImGui::GetIO();

    KbFonts[0] = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 28.0f, nullptr, &base_ranges[0]);
    io.Fonts->AddFontFromFileTTF("fonts/freeserif.ttf", 34.0f, &config, &symbol_ranges[0]);
    io.Fonts->AddFontFromFileTTF("fonts/fontawesome-webfont.ttf", 28.0f, &config, &icon_ranges[0]);
    io.Fonts->Build();

    KbFonts[1] = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 24.0f, nullptr, &base_ranges[0]);
    io.Fonts->AddFontFromFileTTF("fonts/freeserif.ttf", 24.0f, &config, &symbol_ranges[0]);
    io.Fonts->AddFontFromFileTTF("fonts/fontawesome-webfont.ttf", 24.0f, &config, &icon_ranges[0]);
    io.Fonts->Build();

    KbFonts[2] = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 20.0f, nullptr, &base_ranges[0]);
    io.Fonts->AddFontFromFileTTF("fonts/freeserif.ttf", 20.0f, &config, &symbol_ranges[0]);
    io.Fonts->AddFontFromFileTTF("fonts/fontawesome-webfont.ttf", 20.0f, &config, &icon_ranges[0]);
    io.Fonts->Build();

    KbFonts[3] = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 16.0f, nullptr, &base_ranges[0]);
    io.Fonts->AddFontFromFileTTF("fonts/freeserif.ttf", 16.0f, &config, &symbol_ranges[0]);
    io.Fonts->AddFontFromFileTTF("fonts/fontawesome-webfont.ttf", 16.0f, &config, &icon_ranges[0]);
    io.Fonts->Build();

}

static void key_render(xcore::keyboard_resource_t const* kb, xcore::keygroup_resource_t const& kg, xcore::key_resource_t const& key, float globalscale, float x, float y, float r, bool highlight)
{
    const float sw       = key.m_sw * kg.m_sw * kb->m_sw * kb->m_scale * globalscale;
    const float sh       = key.m_sh * kg.m_sh * kb->m_sh * kb->m_scale * globalscale;
    const float kw       = (key.m_w * kg.m_w * kb->m_w * kb->m_scale * globalscale) - (2 * sw);
    const float kh       = (key.m_h * kg.m_h * kb->m_h * kb->m_scale * globalscale) - (2 * sh);
    const float rounding = kw / sw;
    const float hw       = kw / 2;
    const float hh       = kh / 2;
    const float th       = 10.0f * globalscale;

    const xcore::u8* capcolor = kb->m_capcolor;
    const xcore::u8* txtcolor = kb->m_txtcolor;
    const xcore::u8* ledcolor = kb->m_ledcolor;

    if (kg.m_capcolor)
        capcolor = kg.m_capcolor;
    if (kg.m_txtcolor)
        txtcolor = kg.m_txtcolor;
    if (kg.m_ledcolor)
        ledcolor = kg.m_ledcolor;

    if (key.m_capcolor)
        capcolor = key.m_capcolor;
    if (key.m_txtcolor)
        txtcolor = key.m_txtcolor;
    if (key.m_ledcolor)
        ledcolor = key.m_ledcolor;

    ImVec4 dkeycapcolor(capcolor);
    ImVec4 dkeyledcolor(ledcolor);

    const ImU32 rkeycapcolor  = ImColor(capcolor);
    const ImU32 rkeyhltcolor  = ImColor(Lighten(dkeycapcolor, 0.5f));
    const ImU32 rkeyledcolor1 = ImColor(DarkenAlpha(dkeyledcolor, 0.2f, 0.5f));
    const ImU32 rkeyledcolor2 = ImColor(DarkenAlpha(dkeyledcolor, 0.1f, 0.25f));
    const ImU32 rkeyledcolor3 = ImColor(ledcolor);
    const ImU32 rkeytxtcolor  = ImColor(txtcolor);

    // draw_list->AddRect(ImVec2(x, y), ImVec2(x + sz, y + sz), rkeyledcolor, rounding, ImDrawFlags_RoundCornersAll, th/1.0f);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImRotation  rotation(draw_list);

    if (!highlight)
    {
        draw_list->AddRect(ImVec2(x - hw, y - hh), ImVec2(x + hw, y + hh), (rkeyledcolor1), rounding, ImDrawFlags_None, th / 1.0f);
        draw_list->AddRect(ImVec2(x - hw, y - hh), ImVec2(x + hw, y + hh), (rkeyledcolor2), rounding, ImDrawFlags_None, th / 2.0f);
        draw_list->AddRect(ImVec2(x - hw, y - hh), ImVec2(x + hw, y + hh), (rkeyledcolor3), rounding, ImDrawFlags_None, th / 3.0f);
    }
    draw_list->AddRectFilled(ImVec2(x - hw, y - hh), ImVec2(x + hw, y + hh), rkeycapcolor, rounding, ImDrawFlags_RoundCornersAll);

    if (highlight)
    {
        draw_list->AddRect(ImVec2(x - (hw - (th / 4.0f)), y - (hh - (th / 4.0f))), ImVec2(x + (hw - (th / 4.0f)), y + (hh - (th / 4.0f))), (rkeyhltcolor), rounding, ImDrawFlags_None, th / 2.0f);
    }

    if (key.m_label != nullptr)
    {
        char  text[128];
        char* lines[4] = {nullptr, nullptr, nullptr, nullptr};

        int num_lines      = 0;
        int i              = 0;
        lines[num_lines++] = text;
        while (key.m_label[i] != 0 && num_lines < 4)
        {
            if (key.m_label[i] == ' ')
            {
                text[i]            = 0;
                lines[num_lines++] = (text + i + 1);
                text[i + 1]        = 0;
            }
            else
            {
                text[i]     = key.m_label[i];
                text[i + 1] = 0;
            }
            i++;
        }

        if (num_lines == 1)
        {
            ImVec2 td = SelectFontSize(key.m_label, kw);
            draw_list->AddText(ImVec2(x - (td.x / 2), y - (td.y / 2)), rkeytxtcolor, key.m_label);
            ImGui::PopFont();
        }
        else if (num_lines == 2)
        {
            // Push/Pop another smaller font
            ImVec2 td1 = SelectFontSize(lines[0], kw);
            draw_list->AddText(ImVec2(x - (td1.x / 2), y - (td1.y * 1.1f)), rkeytxtcolor, lines[0]);
            ImGui::PopFont();

            ImVec2 td2 = SelectFontSize(lines[1], kw);
            draw_list->AddText(ImVec2(x - (td2.x / 2), y + (td2.y * 0.1f)), rkeytxtcolor, lines[1]);
            ImGui::PopFont();
        }
    }

    if (key.m_nob)
        draw_list->AddLine(ImVec2(x - (hw * 0.125), y + 0.5f * hh), ImVec2(x + (hw * 0.125), y + 0.5f * hh), ImColor(255, 255, 255, 255), 2);

    rotation.Apply(r);
}

void keyboard_render(xcore::keyboard_resource_t const* kb, float posx, float posy, float mousex, float mousey, float globalscale)
{
    // origin = left/top corner
    float ox = posx + (kb->m_w / 2);
    float oy = posy + (kb->m_h / 2);

    int highlighted_key_index = -1;

    for (int g = 0; g < kb->m_nb_keygroups; g++)
    {
        xcore::keygroup_resource_t const& kg = kb->m_keygroups[g];

        float gx = ox + (kg.m_x * kb->m_scale * globalscale);
        float gy = oy + (kg.m_y * kb->m_scale * globalscale);

        // the keygroup down and right coordinate vectors
        ImVec2 ydir(0.0f, 1.0f);
        ImVec2 xdir(1.0f, 0.0f);

        // for rotating the mouse x/y into the keygroup's coordinate system
        ImVec2 mdir(mousex - gx, mousey - gy);

        float rrad = 0.0f;
        if (kg.m_a > 0 || kg.m_a < 0)
        {
            rrad    = (float)3.141592653f * kg.m_a / 180.0f;
            float s = (float)sin(rrad);
            float c = (float)cos(rrad);
            ydir    = ImRotate(ydir, c, s);
            xdir    = ImRotate(xdir, c, s);
            s       = (float)sin(-rrad);
            c       = (float)cos(-rrad);
            mdir    = ImRotate(mdir, c, s);
        }

        float mx = gx + mdir.x;
        float my = gy + mdir.y;

        float rx = gx;
        float ry = gy;
        int   k  = 0;
        for (int r = 0; r < kg.m_r && highlighted_key_index == -1; r++)
        {
            float cx = rx;
            float cy = ry;

            for (int c = 0; c < kg.m_c && highlighted_key_index == -1; c++)
            {
                xcore::key_resource_t const& kc = kg.m_keys[k++];
                if (IsPointInsideKey(kb, kg, kc, globalscale, cx, cy, mx, my))
                {
                    highlighted_key_index = kc.m_index;
                    break;
                }
                cx += (kb->m_w * kg.m_w) * kb->m_scale * globalscale;
            }

            ry += (kb->m_h * kg.m_h) * kb->m_scale * globalscale;
        }

        rx = gx;
        ry = gy;
        k  = 0;
        for (int r = 0; r < kg.m_r; r++)
        {
            float cx = rx;
            float cy = ry;

            for (int c = 0; c < kg.m_c; c++)
            {
                xcore::key_resource_t const& kc = kg.m_keys[k++];
                key_render(kb, kg, kc, globalscale, cx, cy, rrad, kc.m_index == highlighted_key_index);

                if (kc.m_index == highlighted_key_index)
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);

                    // should we prepare a full description of the key:
                    // - keyboard name
                    // - layer name
                    // - key label
                    // - modifiers
                    // - full keycode
                    // - keygroup name
                    // - keymap index
                    const char* test = "Keyboard: %s\nLayer: %s\nLabel: %s\nModifiers: %s\nKeycode: %s\nKeygroup: %s\nKeymap index: %d";

                    ImGui::Text(test, kb->m_name, "QWERTY", kc.m_label, "None", "KC_Q", kg.m_name, kc.m_index);
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }

                cx += xdir.x * (kb->m_w * kg.m_w) * kb->m_scale * globalscale;
                cy += xdir.y * (kb->m_h * kg.m_h) * kb->m_scale * globalscale;
            }

            rx += ydir.x * (kb->m_w * kg.m_w) * kb->m_scale * globalscale;
            ry += ydir.y * (kb->m_h * kg.m_h) * kb->m_scale * globalscale;
        }
    }
}