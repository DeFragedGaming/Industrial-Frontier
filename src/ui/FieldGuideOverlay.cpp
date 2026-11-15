#include "industrial_frontier/ui/FieldGuideOverlay.hpp"
#include "industrial_frontier/resources/PrimitiveTier.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        constexpr UIColor Background{0.027f, 0.040f, 0.065f, 0.985f};
        constexpr UIColor Header{0.11f, 0.14f, 0.18f, 1.0f};
        constexpr UIColor Panel{0.065f, 0.085f, 0.115f, 1.0f};
        constexpr UIColor Selected{0.18f, 0.18f, 0.12f, 1.0f};
        constexpr UIColor Accent{0.85f, 0.61f, 0.27f, 1.0f};
        constexpr UIColor Text{0.93f, 0.95f, 0.97f, 1.0f};
        constexpr UIColor Muted{0.66f, 0.72f, 0.77f, 1.0f};
        constexpr UIColor Planned{0.53f, 0.77f, 0.66f, 1.0f};
        constexpr int ItemsPerPage = 9;
        constexpr float RowHeight = 37.0f;

        struct GuideMachine
        {
            const char* name;
            const char* purpose;
            const char* status;
        };

        constexpr std::array<GuideMachine, 8> Machines{{
            {"Ore Crusher", "Break mined rock into smaller pieces for mineral liberation.", "Prototype available via P (Stage 10E)"},
            {"Grinding Mill", "Further reduce particle size before mineral separation.", "Prototype available via P (Stage 10E)"},
            {"Magnetic Separator", "Separate suitable magnetic minerals from their gangue.", "Prototype available via P (Stage 10E)"},
            {"Flotation Cell", "Concentrate suitable minerals using selective flotation.", "Prototype available via P (Stage 10E)"},
            {"Roasting Furnace", "Thermally prepare certain mineral feeds before separation.", "Prototype available via P (Stage 10E)"},
            {"Smelting Furnace", "Clay furnace for early copper-bearing minerals.", "Stage 10G primitive furnace available"},
            {"Refinery", "Further purify metal-bearing products into usable metals.", "Planned for later metallurgy"},
            {"Industrial Drill", "Continuously extract ore from permanent surface nodes.", "Planned for Stage 10H"}
        }};

        struct MetallurgyPage
        {
            const char* title;
            const char* heading;
            const char* body;
            const char* notes;
        };

        constexpr std::array<MetallurgyPage, 7> SurvivalPages{{
            {"01 - Your first day", "GATHER > CRAFT > BUILD",
                "Collect loose pebbles and sticks from the ground and trees. Open C to craft a Stone Axe, Pickaxe or Shovel. Use the axe to cut logs, the pickaxe for basic ore and stone, and the shovel for clay soil. Open I for inventory; B for storage. Place a campfire before moving into metalworking.",
                "Start with primitive stone equipment. The Field Guide catalog is reference material, not a list of everything you can mine immediately."},
            {"02 - Stone equipment", "STONE TIER: BASIC GATHERING",
                "Stone Axe: chop trees in 4 strikes. Stone Pickaxe: mine Malachite, Azurite, Cuprite, Native Copper, Gypsum, Halite, Calcite, Dolomite, Graphite, Sulfur, Coal, Lignite, Limestone, Sandstone, Shale and Dolostone. Stone Shovel: collect clay soil more efficiently. All stone tools lose durability with use.",
                "Stone pickaxes cannot mine copper sulfides, iron-bearing hard ores, Cassiterite, Quartz, precious metals or rare specialty minerals."},
            {"03 - Storage and carrying", "MOVE THE AMOUNT YOU NEED",
                "Player carrying capacity begins at 500 kg. Open B, choose x1, x5, x10, HALF or ALL, and drag between inventory and storage. A transfer never removes items if the destination cannot accept them. Item transfers use counts; ore transfers use kilograms.",
                "105 logs weigh 210 kg. Use partial transfer to keep room for ore and clay rather than moving everything at once."},
            {"04 - Tool progression", "STONE > COPPER > INDUSTRIAL",
                "Stone tools gather starter copper minerals. Smelt the recovered copper in a clay furnace, cast 0.25 kg ingots, then craft Copper Axe (2 ingots and 2 sticks), Copper Pickaxe (3 ingots and 2 sticks), or Copper Shovel (1 ingot and 2 sticks). Copper tools last longer and work faster.",
                "Copper pickaxes unlock a defined group of moderate-grade minerals including Chalcopyrite, Bornite, Chalcocite, Covellite, Galena, Sphalerite, Bauxite and Fluorite. They still cannot mine Hematite, Magnetite, Pyrite, Quartz, Cassiterite, gold/silver ores or rare high-tier deposits."},
            {"05 - Placement and control keys", "F PREVIEW > R/Q TURN > F PLACE",
                "Equip any placeable kit in your hotbar: campfire, furnace, wooden chest, or log cabin module. Press F to preview; cyan is valid and red is blocked. Turn with R/Q in 45-degree steps and press F to commit. Press X or Escape to cancel. Aim at a placed object and press X to dismantle. The chest must be empty first. Open a chest with B while looking at it. Otherwise B opens your original starter storage. The starter storage remains compatible with older saves.",
                "Left-click excavation requires a shovel for dirt/clay and a pickaxe for stone. Crafting: C opens; Left/Right switches Survival, Metalwork and Construction; Up/Down selects recipes; 1-6 selects a visible recipe; PageUp/PageDown changes recipe pages; Enter or the CRAFT button makes an item. Click tabs and recipes with the mouse. Furnace: V selects ore; E loads or pours; R adds charcoal; T collects ingots. G guide, I inventory, P processing."},
            {"06 - Build a wooden chest", "CRAFT A CHEST, STORE MATERIALS",
                "Open crafting with C and switch to the Construction tab to select Wooden Chest: 8 Logs and 6 Sticks. Equip its kit on the hotbar, press F to preview and F to place. Aim at the chest and press B to open its independent 30-slot, 1400 kg inventory. Choose x1, x5, x10, Half or All for drag-and-drop transfers. Different chests hold different items, even after saving and loading.",
                "To move a chest, empty it first and press X while aiming at it. You recover its chest kit. Press B away from a chest to access your existing starter storage."},
            {"07 - Log cabin starter kit", "FOUNDATION > WALLS > ROOF",
                "Start with a Log Foundation (6 Logs, 4 Sticks) and place it on flat ground with F. Adjacent foundations snap to a 3 m grid. Craft Log Wall (7 Logs, 3 Sticks), Doorframe (5 Logs, 4 Sticks), or Window Wall (6 Logs, 4 Sticks). Equip the piece, face the foundation and press F for a snapping preview; rotate R/Q to choose an edge; F confirms. Add a Log Roof (6 Logs, 4 Sticks) and corner Supports (3 Logs, 3 Sticks). All placed pieces are saved with their orientation.",
                "This is the starter log construction tier. A doorframe is an open passage, not a functioning door yet. Larger roofs, doors, stairs and building-condition systems belong to later construction upgrades."}
        }};

        constexpr std::array<MetallurgyPage, 7> MetallurgyPages{{
            {"01 - Copper, tin and lead", "MINERALS, NOT GENERIC ORE",
                "The ORES tab is grouped by metal. Select Copper, Tin or Lead to see exactly which real minerals contain the metal. The clay furnace can recover copper from Native Copper, Malachite, Azurite and Cuprite; tin from Cassiterite; and lead from Cerussite. The amount recovered depends on each batch's actual mineral fractions and chemical composition.",
                "Cassiterite needs a higher mining tier; the copper pickaxe cannot mine it. Chalcopyrite, Bornite, Chalcocite, Covellite and iron-bearing hard ores need advanced metallurgy. Lead fumes and compounds are highly toxic in real life."},
            {"02 - Prepare charcoal", "CAMPFIRE > CHARCOAL",
                "Build a campfire from 6 Raw Stone, 2 Wood Logs and 3 Wooden Sticks. Ignite with E, cover the burning fire with Clay Soil using R, and collect charcoal with R when finished. Each charcoal provides 45 seconds of clay furnace heat.",
                "A large ore batch needs more charcoal; you can add several fuel units to the furnace."},
            {"03 - Furnace and casting molds", "CRAFTING EQUIPMENT",
                "Press C to craft the Clay Furnace Kit: Clay Soil x20, Raw Stone x8, Sticks x6, Logs x4. Press F to preview, R/Q to rotate, then F again to place on level ground. A single Ingot Mold needs 6 Raw Stone and 8 Clay Soil. The Eight-Ingot Casting Tray needs 12 Raw Stone, 16 Clay Soil and 2 Sticks (recipe 10).",
                "The mold is reusable and stays installed in the furnace. The casting tray holds up to 8 ingots at a time."},
            {"04 - Select and load real ore", "ONE CHARGE: YOUR SELECTED BATCH",
                "Stand near the furnace with mined ore in inventory. Look at the furnace and press V to cycle ore batches; the HUD shows the selected mineral name and mass. Press E to load that entire selected batch when there is sufficient reservoir space. Press R to add charcoal. Smelting time increases with ore mass instead of using a 1 kg per-click limit.",
                "Each batch yields a single supported metal based on its recoverable mineral content. Different molten metals cannot be mixed in the same furnace reservoir. If under 250 g of a metal remains, cool the furnace and press U to discard that unusable residue before switching metal types."},
            {"05 - Cast ingots", "E TO CAST; T TO COLLECT",
                "When at least 250 g of recovered metal is ready, press E while looking at the furnace. The first pour automatically installs an available mold from inventory, prioritizing the Eight-Ingot Casting Tray. The mold stays in the furnace while the metal cools for 30 seconds. Press T to collect the finished ingots directly into inventory. Press E again to refill the same mold.",
                "A single mold casts 1 ingot. The casting tray casts up to 8 ingots in one pour, if enough recovered metal is available. Each ingot represents 250 g of recovered metal."},
            {"06 - Recover the mold", "REUSE CASTING EQUIPMENT",
                "After collecting a casting batch, the emptied mold remains installed. Continue pressing E to cast from the remaining metal reservoir. When the furnace is unlit, has no active ore charge and no cast in progress, press T while facing it to retrieve your reusable mold or casting tray.",
                "The mold is no longer transferred between hot, cooled and inventory states for each casting. Existing legacy hot Copper Molds can still complete and be collected."},
            {"07 - Craft copper tools", "METALWORKING PROGRESSION",
                "Copper Ingots enable the Copper Axe (2 ingots and 2 sticks), Copper Pickaxe (3 ingots and 2 sticks) and Copper Shovel (1 ingot and 2 sticks). A copper pickaxe unlocks several medium-grade minerals but not Cassiterite, Quartz, Hematite or Magnetite. Tin and Lead Ingots are new metallurgy outputs; recipes using them will come later.",
                "The clay furnace is limited to suitable early non-sulfide metal-bearing minerals. Iron and sulfides require industrial upgrades."}
        }};

        template <std::size_t N>
        constexpr bool PagesComplete(const std::array<MetallurgyPage, N>& pages)
        {
            for (const auto& page : pages)
            {
                if (page.title == nullptr || page.heading == nullptr ||
                    page.body == nullptr || page.notes == nullptr)
                    return false;
            }
            return true;
        }

        static_assert(PagesComplete(SurvivalPages));
        static_assert(PagesComplete(MetallurgyPages));

        struct Layout
        {
            float x = 0.0f;
            float y = 0.0f;
            float w = 0.0f;
            float h = 0.0f;
            float tabsY = 0.0f;
            float searchY = 0.0f;
            float listX = 0.0f;
            float listY = 0.0f;
            float listW = 0.0f;
            float detailX = 0.0f;
            float detailW = 0.0f;
        };

        Layout MakeLayout(int width, int height)
        {
            Layout layout;
            layout.w = std::max(0.0f, std::min(1160.0f,
                static_cast<float>(width) - 30.0f));
            layout.h = std::max(0.0f, std::min(690.0f,
                static_cast<float>(height) - 26.0f));
            layout.x = (static_cast<float>(width) - layout.w) * 0.5f;
            layout.y = (static_cast<float>(height) - layout.h) * 0.5f;
            layout.tabsY = layout.y + 70.0f;
            layout.searchY = layout.y + 115.0f;
            layout.listX = layout.x + 18.0f;
            layout.listY = layout.y + 163.0f;
            layout.listW = std::clamp(layout.w * 0.305f, 244.0f, 346.0f);
            layout.detailX = layout.listX + layout.listW + 16.0f;
            layout.detailW = std::max(0.0f,
                layout.x + layout.w - 18.0f - layout.detailX);
            return layout;
        }

        bool In(float x, float y, float left, float top, float width, float height)
        {
            return x >= left && x <= left + width &&
                   y >= top && y <= top + height;
        }

        std::string Ascii(std::string_view text)
        {
            std::string result;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const unsigned char c = static_cast<unsigned char>(text[i]);
                if (c < 128)
                {
                    result += static_cast<char>(c);
                }
                else if (c == 0xC3 && i + 1 < text.size())
                {
                    const unsigned char next = static_cast<unsigned char>(text[++i]);
                    switch (next)
                    {
                        case 0xA4: case 0x84: result += 'a'; break;
                        case 0xB6: case 0x96: result += 'o'; break;
                        case 0xBC: case 0x9C: result += 'u'; break;
                        case 0xA9: case 0x89: result += 'e'; break;
                        case 0xA8: case 0x88: result += 'e'; break;
                        case 0xB1: case 0x91: result += 'n'; break;
                        case 0x9F: result += "ss"; break;
                        default: result += '?'; break;
                    }
                }
                else if (c == 0xC2 && i + 1 < text.size())
                {
                    const unsigned char next = static_cast<unsigned char>(text[++i]);
                    if (next == 0xB7)
                    {
                        result += '.';
                    }
                }
                else if ((c & 0xC0) != 0x80)
                {
                    result += '?';
                }
            }
            return result;
        }

        std::string Lower(std::string_view text)
        {
            std::string value = Ascii(text);
            for (char& character : value)
            {
                character = static_cast<char>(std::tolower(
                    static_cast<unsigned char>(character)));
            }
            return value;
        }

        bool Matches(std::string_view text, std::string_view query)
        {
            return Lower(text).find(Lower(query)) != std::string::npos;
        }

        std::string Joined(const std::vector<std::string>& values)
        {
            std::string result;
            for (const auto& value : values)
            {
                if (!result.empty())
                {
                    result += ", ";
                }
                result += value;
            }
            return result.empty() ? "Not specified" : result;
        }

        std::string Number(float value, int decimals = 1)
        {
            std::ostringstream output;
            output << std::fixed << std::setprecision(decimals) << value;
            return output.str();
        }

        std::vector<std::string> Wrapped(UIRenderer& ui, std::string_view text,
                                        float scale, float width)
        {
            const std::string safe = Ascii(text);
            std::istringstream stream(safe);
            std::string word;
            std::string line;
            std::vector<std::string> lines;
            const float limit = std::max(30.0f, width);
            while (stream >> word)
            {
                const std::string candidate = line.empty() ? word : line + " " + word;
                if (!line.empty() && ui.MeasureText(candidate, scale) > limit)
                {
                    lines.push_back(line);
                    line.clear();
                }
                if (ui.MeasureText(word, scale) > limit)
                {
                    std::string part;
                    for (char character : word)
                    {
                        if (!part.empty() && ui.MeasureText(part + character, scale) > limit)
                        {
                            lines.push_back(part);
                            part.clear();
                        }
                        part += character;
                    }
                    line = part;
                }
                else
                {
                    line = line.empty() ? word : line + " " + word;
                }
            }
            if (!line.empty())
            {
                lines.push_back(line);
            }
            return lines;
        }

        float WriteWrapped(UIRenderer& ui, std::string_view text,
                           float x, float y, float width, float scale,
                           float lineHeight, float bottom, UIColor color)
        {
            for (const auto& line : Wrapped(ui, text, scale, width))
            {
                if (y + lineHeight > bottom)
                {
                    break;
                }
                ui.DrawText(line, x, y, scale, color);
                y += lineHeight;
            }
            return y;
        }

        void WriteFitted(UIRenderer& ui, const std::string& text,
                         float x, float y, float width, UIColor color,
                         float scale = 0.98f)
        {
            const std::string safe = Ascii(text);
            while (scale > 0.72f && ui.MeasureText(safe, scale) > width)
            {
                scale -= 0.05f;
            }
            if (ui.MeasureText(safe, scale) <= width)
            {
                ui.DrawText(safe, x, y, scale, color);
                return;
            }
            std::string shortened = safe;
            while (!shortened.empty() &&
                   ui.MeasureText(shortened + "..", scale) > width)
            {
                shortened.pop_back();
            }
            ui.DrawText(shortened + "..", x, y, scale, color);
        }

        float Labeled(UIRenderer& ui, const std::string& label,
                      const std::string& value, float x, float y, float width,
                      float bottom, UIColor color = Text)
        {
            if (y + 38.0f >= bottom)
            {
                return bottom;
            }
            ui.DrawText(label, x, y, 0.86f, Accent);
            return WriteWrapped(ui, value, x, y + 17.0f, width,
                0.96f, 19.0f, bottom, color) + 12.0f;
        }

        void SectionTabs(UIRenderer& ui, const Layout& l, FieldGuideSection section)
        {
            const char* labels[5] = {"ORES", "START HERE", "METALWORK", "PROCESSING", "MACHINES"};
            const auto index = static_cast<int>(section);
            for (int i = 0; i < 5; ++i)
            {
                const float x = l.x + 18.0f + static_cast<float>(i) * 132.0f;
                ui.DrawRectangle(x, l.tabsY, 124.0f, 34.0f,
                    index == i ? Selected : Header);
                if (index == i)
                {
                    ui.DrawRectangle(x, l.tabsY + 31.0f, 124.0f, 3.0f, Accent);
                }
                WriteFitted(ui, labels[i], x + 9.0f, l.tabsY + 9.0f,
                    108.0f, index == i ? Text : Muted, 0.95f);
            }
        }
    }

    void FieldGuideOverlay::Reset()
    {
        m_section = FieldGuideSection::Survival;
        m_resourceFilter = FieldGuideResourceFilter::AllResources;
        m_selected = 0;
        m_query.clear();
        m_searchFocused = false;
    }

    void FieldGuideOverlay::ChangeSection(FieldGuideSection section)
    {
        m_section = section;
        m_selected = 0;
        m_query.clear();
        m_searchFocused = false;
    }

    bool FieldGuideOverlay::SearchFocused() const
    {
        return m_searchFocused;
    }

    const std::string& FieldGuideOverlay::Query() const
    {
        return m_query;
    }

    FieldGuideResourceFilter FieldGuideOverlay::ResourceFilter() const
    {
        return m_resourceFilter;
    }

    std::vector<FieldGuideOverlay::Entry> FieldGuideOverlay::Filtered(
        const MineralDatabase& minerals, const FieldGuideCatalog& routes) const
    {
        std::vector<Entry> result;
        if (m_section == FieldGuideSection::Resources)
        {
            constexpr std::array<std::string_view, 28> Metals{{
                "Aluminum", "Antimony", "Chromium", "Cobalt", "Copper", "Gold",
                "Iron", "Lead", "Lithium", "Magnesium", "Manganese", "Mercury",
                "Molybdenum", "Nickel", "Niobium", "Rare Earths", "Silver",
                "Strontium", "Tantalum", "Tin", "Titanium", "Tungsten",
                "Uranium", "Vanadium", "Zinc", "Zirconium", "Arsenic", "Barium"
            }};
            for (const auto metal : Metals)
            {
                const auto sources = minerals.FindByCommodity(metal);
                if (sources.empty())
                    continue;
                bool matches = Matches(metal, m_query);
                for (const auto* mineral : sources)
                    matches = matches || Matches(mineral->name, m_query) ||
                        Matches(mineral->formula, m_query) || Matches(mineral->id, m_query);
                if (matches)
                {
                    Entry entry;
                    entry.title = std::string(metal);
                    entry.metalGroup = std::string(metal);
                    result.push_back(std::move(entry));
                }
            }
        }
        else if (m_section == FieldGuideSection::Processing)
        {
            for (const auto& route : routes.All())
            {
                if (Matches(route.title, m_query) || Matches(route.products, m_query) ||
                    Matches(route.id, m_query) || Matches(route.process, m_query))
                {
                    result.push_back({route.title, nullptr, &route, 0});
                }
            }
        }
        else if (m_section == FieldGuideSection::Metallurgy ||
                 m_section == FieldGuideSection::Survival)
        {
            const std::size_t total = m_section == FieldGuideSection::Survival ?
                SurvivalPages.size() : MetallurgyPages.size();
            for (std::size_t i = 0; i < total; ++i)
            {
                const auto& page = m_section == FieldGuideSection::Survival ?
                    SurvivalPages[i] : MetallurgyPages[i];
                if (Matches(page.title, m_query) || Matches(page.heading, m_query) ||
                    Matches(page.body, m_query) || Matches(page.notes, m_query))
                    result.push_back({page.title, nullptr, nullptr, i});
            }
        }
        else
        {
            for (std::size_t i = 0; i < Machines.size(); ++i)
            {
                if (Matches(Machines[i].name, m_query) ||
                    Matches(Machines[i].purpose, m_query))
                {
                    result.push_back({Machines[i].name, nullptr, nullptr, i});
                }
            }
        }
        if (m_section != FieldGuideSection::Metallurgy &&
            m_section != FieldGuideSection::Survival)
            std::sort(result.begin(), result.end(), [](const Entry& a, const Entry& b)
            {
                return Lower(a.title) < Lower(b.title);
            });
        return result;
    }

    void FieldGuideOverlay::MoveSelection(int amount,
        const MineralDatabase& minerals, const FieldGuideCatalog& routes)
    {
        const auto filtered = Filtered(minerals, routes);
        if (filtered.empty())
        {
            m_selected = 0;
            return;
        }
        const auto current = static_cast<long long>(std::min(m_selected,
            filtered.size() - 1));
        const auto destination = std::clamp(current + amount, 0LL,
            static_cast<long long>(filtered.size() - 1));
        m_selected = static_cast<std::size_t>(destination);
    }

    void FieldGuideOverlay::KeyDown(SDL_Scancode key,
        const MineralDatabase& minerals, const FieldGuideCatalog& routes)
    {
        if (key == SDL_SCANCODE_BACKSPACE && m_searchFocused)
        {
            if (!m_query.empty())
            {
                m_query.pop_back();
                m_selected = 0;
            }
        }
        else if (key == SDL_SCANCODE_ESCAPE && m_searchFocused)
        {
            m_searchFocused = false;
        }
        else if (key == SDL_SCANCODE_TAB)
        {
            const int next = (static_cast<int>(m_section) + 1) % 5;
            ChangeSection(static_cast<FieldGuideSection>(next));
        }
        else if (key == SDL_SCANCODE_DOWN)
        {
            MoveSelection(1, minerals, routes);
        }
        else if (key == SDL_SCANCODE_UP)
        {
            MoveSelection(-1, minerals, routes);
        }
        else if (key == SDL_SCANCODE_PAGEDOWN || key == SDL_SCANCODE_RIGHT)
        {
            MoveSelection(ItemsPerPage, minerals, routes);
        }
        else if (key == SDL_SCANCODE_PAGEUP || key == SDL_SCANCODE_LEFT)
        {
            MoveSelection(-ItemsPerPage, minerals, routes);
        }
        else if (key == SDL_SCANCODE_HOME)
        {
            m_selected = 0;
        }
        else if (key == SDL_SCANCODE_END)
        {
            const auto filtered = Filtered(minerals, routes);
            m_selected = filtered.empty() ? 0 : filtered.size() - 1;
        }
    }

    void FieldGuideOverlay::TextInput(const char* text)
    {
        if (!m_searchFocused || text == nullptr)
        {
            return;
        }
        for (const char* ptr = text; *ptr != '\0'; ++ptr)
        {
            const unsigned char character = static_cast<unsigned char>(*ptr);
            if (m_query.size() >= 44)
            {
                break;
            }
            if ((character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') ||
                character == ' ' || character == '-' || character == '(' ||
                character == ')' || character == '+')
            {
                m_query.push_back(static_cast<char>(character));
                m_selected = 0;
            }
        }
    }

    void FieldGuideOverlay::Wheel(float direction,
        const MineralDatabase& minerals, const FieldGuideCatalog& routes)
    {
        if (direction > 0.0f)
        {
            MoveSelection(-3, minerals, routes);
        }
        else if (direction < 0.0f)
        {
            MoveSelection(3, minerals, routes);
        }
    }

    bool FieldGuideOverlay::Click(float x, float y, int width, int height,
        const MineralDatabase& minerals, const FieldGuideCatalog& routes)
    {
        const auto layout = MakeLayout(width, height);
        if (In(x, y, layout.x + layout.w - 116.0f, layout.y + 16.0f, 97.0f, 34.0f))
        {
            return true;
        }
        for (int i = 0; i < 5; ++i)
        {
            if (In(x, y, layout.x + 18.0f + i * 132.0f,
                layout.tabsY, 124.0f, 34.0f))
            {
                ChangeSection(static_cast<FieldGuideSection>(i));
                return false;
            }
        }
        if (In(x, y, layout.listX, layout.searchY,
            layout.w - 36.0f, 36.0f))
        {
            const float clearX = layout.x + layout.w - 75.0f;
            if (x >= clearX)
            {
                m_query.clear();
                m_selected = 0;
            }
            m_searchFocused = true;
            return false;
        }
        m_searchFocused = false;
        const auto entries = Filtered(minerals, routes);
        const std::size_t page = m_selected / ItemsPerPage;
        if (In(x, y, layout.listX, layout.listY,
            layout.listW, RowHeight * ItemsPerPage))
        {
            const std::size_t row = static_cast<std::size_t>(
                (y - layout.listY) / RowHeight);
            const std::size_t choice = page * ItemsPerPage + row;
            if (choice < entries.size())
            {
                m_selected = choice;
            }
        }
        const float footerY = layout.listY + ItemsPerPage * RowHeight + 12.0f;
        if (In(x, y, layout.listX, footerY, 90.0f, 34.0f))
        {
            MoveSelection(-ItemsPerPage, minerals, routes);
        }
        if (In(x, y, layout.listX + layout.listW - 90.0f,
            footerY, 90.0f, 34.0f))
        {
            MoveSelection(ItemsPerPage, minerals, routes);
        }
        return false;
    }

    void FieldGuideOverlay::Render(UIRenderer& ui,
        const MineralDatabase& minerals, const FieldGuideCatalog& routes,
        int width, int height) const
    {
        const Layout l = MakeLayout(width, height);
        if (l.w < 730.0f || l.h < 560.0f)
        {
            ui.DrawRectangle(12.0f, 12.0f,
                std::max(0.0f, static_cast<float>(width) - 24.0f), 80.0f, Background);
            ui.DrawText("FIELD GUIDE: INCREASE WINDOW SIZE", 28.0f,
                42.0f, 1.0f, Text);
            return;
        }

        ui.DrawRectangle(l.x - 2.0f, l.y - 2.0f, l.w + 4.0f, l.h + 4.0f, Header);
        ui.DrawRectangle(l.x, l.y, l.w, l.h, Background);
        ui.DrawRectangle(l.x, l.y, l.w, 63.0f, Header);
        ui.DrawRectangle(l.x, l.y, 4.0f, l.h, Accent);
        ui.DrawText("INDUSTRIAL FIELD GUIDE", l.x + 20.0f,
            l.y + 17.0f, 1.46f, Text);
        ui.DrawRectangle(l.x + l.w - 116.0f, l.y + 16.0f,
            97.0f, 34.0f, Selected);
        ui.DrawText("G / CLOSE", l.x + l.w - 106.0f,
            l.y + 25.0f, 0.88f, Text);

        SectionTabs(ui, l, m_section);
        ui.DrawRectangle(l.listX, l.searchY, l.w - 36.0f, 36.0f,
            m_searchFocused ? Selected : Panel);
        ui.DrawText(m_query.empty() ? "SEARCH: CLICK HERE TO TYPE" :
            "SEARCH: " + Ascii(m_query) + (m_searchFocused ? "_" : ""),
            l.listX + 12.0f, l.searchY + 10.0f, 0.95f,
            m_searchFocused ? Text : Muted);
        ui.DrawText("CLEAR", l.x + l.w - 71.0f,
            l.searchY + 10.0f, 0.86f, Accent);

        const auto entries = Filtered(minerals, routes);
        const std::size_t chosen = entries.empty() ? 0 :
            std::min(m_selected, entries.size() - 1);
        const std::size_t page = chosen / ItemsPerPage;
        const std::size_t pages = std::max<std::size_t>(1,
            (entries.size() + ItemsPerPage - 1) / ItemsPerPage);
        ui.DrawRectangle(l.listX, l.listY, l.listW,
            ItemsPerPage * RowHeight, Panel);
        for (int row = 0; row < ItemsPerPage; ++row)
        {
            const std::size_t index = page * ItemsPerPage + row;
            if (index >= entries.size())
            {
                break;
            }
            const float itemY = l.listY + row * RowHeight;
            if (chosen == index)
            {
                ui.DrawRectangle(l.listX, itemY, l.listW, RowHeight, Selected);
                ui.DrawRectangle(l.listX, itemY, 3.0f, RowHeight, Accent);
            }
            WriteFitted(ui, entries[index].title, l.listX + 11.0f,
                itemY + 11.0f, l.listW - 23.0f,
                chosen == index ? Text : Muted, 0.99f);
        }
        if (entries.empty())
        {
            WriteWrapped(ui, "No matching entries. Clear the search or change section.",
                l.listX + 12.0f, l.listY + 20.0f, l.listW - 24.0f,
                0.96f, 19.0f, l.listY + 220.0f, Muted);
        }
        const float footerY = l.listY + ItemsPerPage * RowHeight + 12.0f;
        ui.DrawRectangle(l.listX, footerY, 90.0f, 34.0f, Header);
        ui.DrawText("PREV", l.listX + 17.0f, footerY + 9.0f, 0.92f, Text);
        ui.DrawRectangle(l.listX + l.listW - 90.0f,
            footerY, 90.0f, 34.0f, Header);
        ui.DrawText("NEXT", l.listX + l.listW - 73.0f,
            footerY + 9.0f, 0.92f, Text);
        const std::string counter = std::to_string(page + 1) + "/" +
            std::to_string(pages);
        ui.DrawText(counter, l.listX + (l.listW -
            ui.MeasureText(counter, 0.84f)) * 0.5f,
            footerY + 9.0f, 0.84f, Muted);

        if (m_section == FieldGuideSection::Resources)
            WriteFitted(ui, "SELECT A METAL TO VIEW ITS SOURCE ORES",
                l.listX, footerY + 47.0f, l.listW, Muted, 0.82f);

        const float detailsY = l.listY;
        const float detailBottom = l.y + l.h - 28.0f;
        ui.DrawRectangle(l.detailX, detailsY, l.detailW,
            detailBottom - detailsY + 10.0f, Panel);
        ui.DrawRectangle(l.detailX, detailsY, l.detailW, 3.0f, Accent);
        const float x = l.detailX + 18.0f;
        const float contentWidth = l.detailW - 36.0f;
        float y = detailsY + 17.0f;

        if (!entries.empty())
        {
            const auto& item = entries[chosen];
            y = WriteWrapped(ui, item.title, x, y,
                contentWidth, 1.34f, 25.0f, detailBottom, Text) + 12.0f;
            if (m_section == FieldGuideSection::Resources && !item.metalGroup.empty())
            {
                const auto sources = minerals.FindByCommodity(item.metalGroup);
                y = Labeled(ui, "SOURCE MINERALS / GEOLOGICAL DATABASE",
                    std::to_string(sources.size()) + " real mineral entries. Mining and smelting tiers are independent.",
                    x, y, contentWidth, detailBottom);
                for (const auto* mineral : sources)
                {
                    const std::string tier = IsStonePickaxeMineral(mineral->id) ?
                        "STONE" : IsCopperPickaxeMineral(mineral->id) ?
                        "COPPER" : "ADVANCED";
                    const bool primitive = IsPrimitiveCopperMineral(mineral->id) ||
                        mineral->id == "cassiterite" || mineral->id == "cerussite";
                    const std::string line = mineral->name +
                        (mineral->formula.empty() ? "" : " (" + mineral->formula + ")") +
                        " | " + tier + (primitive ? " | CLAY FURNACE" : "");
                    y = WriteWrapped(ui, line, x, y + 4.0f, contentWidth,
                        0.88f, 18.0f, detailBottom - 10.0f,
                        primitive ? Planned : Muted) + 4.0f;
                    if (y + 18.0f >= detailBottom)
                        break;
                }
                if (item.metalGroup == "Iron")
                    y = Labeled(ui, "IRON PRODUCTION", "Requires a future bloomery or industrial furnace; iron is not molten in this primitive casting process.",
                        x, y, contentWidth, detailBottom, Muted);
                else if (item.metalGroup == "Copper" || item.metalGroup == "Tin" ||
                    item.metalGroup == "Lead")
                    y = Labeled(ui, "PRIMITIVE METALLURGY", "Only mineral entries marked CLAY FURNACE can be directly reduced and cast. Others require more advanced processing.",
                        x, y, contentWidth, detailBottom, Muted);
            }
            else if (item.mineral != nullptr)
            {
                const auto& mineral = *item.mineral;
                y = Labeled(ui, "CLASSIFICATION / COMMODITY",
                    std::string(ResourceKindName(mineral.kind)) + " | " +
                    mineral.commodity, x, y, contentWidth, detailBottom);
                if (!mineral.formula.empty())
                {
                    y = Labeled(ui, "CHEMICAL FORMULA",
                        mineral.formula, x, y, contentWidth, detailBottom);
                }
                if (mineral.hasMohsHardness)
                {
                    const std::string hardness = Number(mineral.minimumMohsHardness) +
                        (mineral.maximumMohsHardness > mineral.minimumMohsHardness
                        ? " - " + Number(mineral.maximumMohsHardness) : "");
                    y = Labeled(ui, "MOHS HARDNESS", hardness,
                        x, y, contentWidth, detailBottom);
                }
                const bool stone = IsStonePickaxeMineral(mineral.id);
                const bool copper = IsCopperPickaxeMineral(mineral.id);
                y = Labeled(ui, "MINING TOOL TIER",
                    stone ? "STONE PICKAXE OR BETTER" :
                    copper ? "COPPER PICKAXE OR BETTER" :
                    "ADVANCED PICKAXE REQUIRED (NOT YET AVAILABLE)",
                    x, y, contentWidth, detailBottom,
                    stone || copper ? Planned : Muted);
                if (IsPrimitiveCopperMineral(mineral.id))
                    y = Labeled(ui, "EARLY SMELTING",
                        "ACCEPTED BY CLAY FURNACE", x, y,
                        contentWidth, detailBottom, Planned);
                else if (mineral.commodity == "Copper")
                    y = Labeled(ui, "EARLY SMELTING",
                        "REQUIRES LATER METALLURGY", x, y,
                        contentWidth, detailBottom, Muted);
                y = Labeled(ui, "GEOLOGICAL SETTINGS",
                    Joined(mineral.hostGeologies), x, y, contentWidth, detailBottom);
                y = Labeled(ui, "TARGET ELEMENTS",
                    Joined(mineral.targetElements), x, y, contentWidth, detailBottom);
                const auto* route = routes.FindById(mineral.processingRouteId);
                if (route != nullptr)
                {
                    y = Labeled(ui, "POTENTIAL PRODUCTS / PLANNED",
                        route->products, x, y, contentWidth, detailBottom, Planned);
                    y = Labeled(ui, "FUTURE PROCESSING CHAIN",
                        route->process, x, y, contentWidth, detailBottom);
                }
                if (!mineral.notes.empty())
                {
                    y = Labeled(ui, "GEOLOGICAL NOTES",
                        mineral.notes, x, y, contentWidth, detailBottom, Muted);
                }
            }
            else if (item.route != nullptr)
            {
                const auto& route = *item.route;
                const bool earlyCopper = route.id == "copper_oxide" ||
                    route.id == "copper_sulfide" || route.id == "native_metal";
                y = Labeled(ui, "STATUS", earlyCopper ?
                    "PARTIAL: EARLY COPPER FURNACE NOW AVAILABLE" :
                    "PLANNED - NOT YET CRAFTABLE",
                    x, y, contentWidth, detailBottom, Planned);
                y = Labeled(ui, "POTENTIAL PRODUCTS", route.products,
                    x, y, contentWidth, detailBottom);
                y = Labeled(ui, "PROCESSING SEQUENCE", route.process,
                    x, y, contentWidth, detailBottom);
                y = Labeled(ui, "NOTES", route.notes,
                    x, y, contentWidth, detailBottom, Muted);
                std::size_t sources = 0;
                std::string examples;
                for (const auto& mineral : minerals.All())
                {
                    if (mineral.processingRouteId == route.id)
                    {
                        ++sources;
                        if (sources <= 4)
                        {
                            if (!examples.empty())
                            {
                                examples += ", ";
                            }
                            examples += mineral.name;
                        }
                    }
                }
                y = Labeled(ui, "EXAMPLE INPUT MATERIALS",
                    examples, x, y, contentWidth, detailBottom);
            }
            else if (m_section == FieldGuideSection::Metallurgy ||
                     m_section == FieldGuideSection::Survival)
            {
                const auto& guidePage = m_section == FieldGuideSection::Survival ?
                    SurvivalPages[item.machineIndex] : MetallurgyPages[item.machineIndex];
                y = Labeled(ui, "STEP-BY-STEP INSTRUCTIONS", guidePage.heading,
                    x, y, contentWidth, detailBottom);
                y = Labeled(ui, "WHAT TO DO", guidePage.body,
                    x, y, contentWidth, detailBottom);
                y = Labeled(ui, "GEOLOGY / SAFETY NOTES", guidePage.notes,
                    x, y, contentWidth, detailBottom, Muted);
            }
            else
            {
                const auto& machine = Machines[item.machineIndex];
                y = Labeled(ui, "IMPLEMENTATION STATUS", machine.status,
                    x, y, contentWidth, detailBottom, Planned);
                y = Labeled(ui, "PURPOSE", machine.purpose,
                    x, y, contentWidth, detailBottom);
                y = Labeled(ui, "RECIPES", "Prototype processing equipment is not yet placeable. Additional machine recipes are planned for later stages.",
                    x, y, contentWidth, detailBottom, Muted);
            }
        }
        if (l.y + l.h - 15.0f > footerY + 45.0f)
        {
            ui.DrawText("TAB: SECTION   ARROWS: SELECT   CLICK METAL: VIEW SOURCE MINERALS",
                l.x + 20.0f, l.y + l.h - 17.0f, 0.82f, Muted);
        }
    }
}
