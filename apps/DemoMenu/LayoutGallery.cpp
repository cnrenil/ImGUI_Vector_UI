#include "LayoutGallery.h"

namespace Demo
{
    void LayoutGallery::Render(IVUI::UI& ui)
    {
        ui.widgets().Combo("Layout", &mode_, {"Tiled", "Columns", "Windows"});
        if (mode_ == 2 && ui.widgets().Button("Reopen Windows"))
            for (bool& open : open_) open = true;

        const IVUI::ContentBlock blocks[] = {
            {"inputs", "Controls", [&](IVUI::UI& owner) {
                owner.widgets().TextInput("Name", name_, sizeof(name_));
                owner.widgets().Toggle("Enabled", &enabled_);
                owner.widgets().SliderFloat("Intensity", &intensity_, 0, 1, "%.2f");
                owner.widgets().Combo("Quality", &quality_, {"Draft", "Standard", "High"});
            }, 1.0f, &open_[0]},
            {"palette", "Palette", [&](IVUI::UI& owner) {
                owner.widgets().ColorPicker("Color", &color_);
                owner.widgets().Checkbox("Enabled", &enabled_);
                owner.widgets().ProgressBar("Level", intensity_);
            }, 1.0f, &open_[1]},
            {"activity", "Activity", [&](IVUI::UI& owner) {
                owner.widgets().AnimatedText("Connected##activity");
                owner.widgets().ProgressBar("Processing", intensity_);
                if (owner.widgets().Button("Run", {.prominent = true}))
                {
                    IVUI::NotificationOptions options;
                    options.type = IVUI::NotificationType::Success;
                    owner.widgets().Notify("Run complete", name_, options);
                }
            }, 1.0f, &open_[2]},
            {"details", "Details", [&](IVUI::UI&) {
                if (ImGui::BeginTable("details", 2, ImGuiTableFlags_RowBg))
                {
                    const char* keys[] = {"Name", "Status", "Quality"};
                    const char* values[] = {name_, enabled_ ? "Enabled" : "Disabled",
                        quality_ == 0 ? "Draft" : quality_ == 1 ? "Standard" : "High"};
                    for (int row = 0; row < 3; ++row)
                    {
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(keys[row]);
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(values[row]);
                    }
                    ImGui::EndTable();
                }
            }, 1.0f, &open_[3]}
        };
        IVUI::ContentLayoutOptions layout;
        layout.mode = static_cast<IVUI::ContentLayout>(mode_);
        layout.embedded = true;
        layout.minimumColumnWidth = 220;
        layout.tileHeight = 260;
        layout.windowFlags = ImGuiWindowFlags_NoSavedSettings;
        ui.layouts().Content("gallery", blocks, layout);
    }
}
