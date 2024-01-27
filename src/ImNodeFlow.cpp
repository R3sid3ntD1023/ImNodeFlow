#include "ImNodeFlow.h"

namespace ImFlow
{
    // -----------------------------------------------------------------------------------------------------------------
    // LINK

    void Link::update()
    {
        ImVec2 start = m_left->pinPoint();
        ImVec2 end  = m_right->pinPoint();
        float thickness = m_inf->style().link_thickness;

        if (smart_bezier_collider(ImGui::GetMousePos(), start, end, 2.5))
        {
            m_hovered = true;
            thickness = m_inf->style().link_hovered_thickness;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                m_selected = true;
        }
        else { m_hovered = false; }

        if (m_selected)
            smart_bezier(start, end, m_inf->style().colors.link_selected_outline, thickness + m_inf->style().link_selected_outline_thickness);
        smart_bezier(start, end, m_inf->style().colors.link, thickness);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // BASE NODE

    bool BaseNode::hovered()
    {
        return ImGui::IsMouseHoveringRect(m_inf->canvas2screen(m_pos - m_paddingTL), m_inf->canvas2screen(m_pos + m_size + m_paddingBR));
    }

    void BaseNode::update(ImVec2& offset)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImGui::PushID(this);

        draw_list->ChannelsSetCurrent(1); // Foreground
        ImGui::SetCursorScreenPos(offset + m_pos);

        ImGui::BeginGroup();

        // Header
        ImGui::BeginGroup();
        ImGui::TextColored(m_inf->style().colors.node_header_title, m_name.c_str());
        ImGui::Spacing();
        ImGui::EndGroup();
        float headerH = ImGui::GetItemRectSize().y;

        // Inputs
        ImGui::BeginGroup();
        for(auto& p : m_ins)
        {
            p->pos(ImGui::GetCursorPos() + ImGui::GetWindowPos());
            p->update();
        }
        ImGui::EndGroup();
        ImGui::SameLine();

        // Content
        ImGui::BeginGroup();
        draw();
        ImGui::Dummy(ImVec2(0.f, 0.f));
        ImGui::EndGroup();
        ImGui::SameLine();

        // Outputs
        float maxW = 0.0f;
        for (auto& p : m_outs)
        {
            float w = p->calcWidth();
            if (w > maxW)
                maxW = w;
        }
        ImGui::BeginGroup();
        for (auto& p : m_outs)
        {
            p->pos(ImGui::GetCursorPos() + ImGui::GetWindowPos() + ImVec2(maxW - p->calcWidth(), 0.f));
            p->update();
        }
        ImGui::EndGroup();

        ImGui::EndGroup();

        // Background
        m_size = ImGui::GetItemRectSize();
        ImVec2 headerSize = ImVec2(m_size.x + m_paddingTL.x, headerH);
        draw_list->ChannelsSetCurrent(0);
        draw_list->AddRectFilled(offset + m_pos - m_paddingTL, offset + m_pos + m_size + m_paddingBR, m_inf->style().colors.node_bg, m_inf->style().node_radius);
        draw_list->AddRectFilled(offset + m_pos - m_paddingTL, offset + m_pos + headerSize, m_inf->style().colors.node_header, m_inf->style().node_radius, ImDrawFlags_RoundCornersTop);
        if(m_selected)
            draw_list->AddRect(offset + m_pos - m_paddingTL, offset + m_pos + m_size + m_paddingBR, m_inf->style().colors.node_selected_border, m_inf->style().node_radius, 0, m_inf->style().node_border_selected_thickness);
        else
            draw_list->AddRect(offset + m_pos - m_paddingTL, offset + m_pos + m_size + m_paddingBR, m_inf->style().colors.node_border, m_inf->style().node_radius, 0, m_inf->style().node_border_thickness);


        if (hovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            m_selected = true;

        bool onHeader = ImGui::IsMouseHoveringRect(offset + m_pos - m_paddingTL, offset + m_pos + headerSize);
        if (onHeader && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_dragged = true;
            m_inf->draggingNode(true);
            m_posOld = m_pos;
        }
        if(m_dragged || (m_selected && m_inf->draggingNode()))
        {
            float step = m_inf->style().grid_size / m_inf->style().grid_subdivisions;
            ImVec2 wantedPos = m_posOld + ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.f);
            // "Slam" The position
            m_pos.x = step * (int)(wantedPos.x / step);
            m_pos.y = step * (int)(wantedPos.y / step);

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                m_dragged = false;
                m_inf->draggingNode(false);
                m_posOld = m_pos;
            }
        }
        ImGui::PopID();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // HANDLER

    int ImNodeFlow::m_instances = 0;

    bool ImNodeFlow::on_selected_node()
    {
        return std::any_of(m_nodes.begin(), m_nodes.end(),
                           [](auto& n) { return n->selected() && n->hovered();});
    }

    bool ImNodeFlow::on_free_space()
    {
        return std::all_of(m_nodes.begin(), m_nodes.end(),
                    [](auto& n) {return !n->hovered();})
               && std::all_of(m_links.begin(), m_links.end(),
                    [](auto& l) {return !l.lock()->hovered();});
    }

    ImVec2 ImNodeFlow::canvas2screen(const ImVec2 &p)
    {
        return p + m_scroll + ImGui::GetWindowPos();
    }

    ImVec2 ImNodeFlow::screen2canvas(const ImVec2 &p)
    {
        return p - m_pos - m_scroll;
    }

    void ImNodeFlow::createLink(Pin* left, Pin* right)
    {
        right->createLink(left);
        m_links.emplace_back(right->getLink());
    }

    void ImNodeFlow::update()
    {
        // Updating looping stuff
        m_hovering = nullptr;
        m_draggingNode = m_draggingNodeNext;

        // Create child canvas
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_style.colors.background);
        ImGui::BeginChild(m_name.c_str(), ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        m_pos = ImGui::GetWindowPos();

        ImVec2 offset = ImGui::GetCursorScreenPos() + m_scroll;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Display grid
        ImVec2 win_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_sz = ImGui::GetWindowSize();
        for (float x = fmodf(m_scroll.x, m_style.grid_size); x < canvas_sz.x; x += m_style.grid_size)
            draw_list->AddLine(ImVec2(x, 0.0f) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, m_style.colors.grid);
        for (float y = fmodf(m_scroll.y, m_style.grid_size); y < canvas_sz.y; y += m_style.grid_size)
            draw_list->AddLine(ImVec2(0.0f, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, m_style.colors.grid);
        for (float x = fmodf(m_scroll.x, m_style.grid_size / m_style.grid_subdivisions); x < canvas_sz.x; x += m_style.grid_size / m_style.grid_subdivisions)
            draw_list->AddLine(ImVec2(x, 0.0f) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, m_style.colors.subGrid);
        for (float y = fmodf(m_scroll.y, m_style.grid_size / m_style.grid_subdivisions); y < canvas_sz.y; y += m_style.grid_size / m_style.grid_subdivisions)
            draw_list->AddLine(ImVec2(0.0f, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, m_style.colors.subGrid);

        //  Deselection (must be done before Nodes and Links update)
        if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            for (auto &l: m_links) { l.lock()->selected(false); }
        if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !on_selected_node())
            for (auto& n : m_nodes) { n->selected(false); }

        // Update and draw nodes
        draw_list->ChannelsSplit(2);
        for (auto& node : m_nodes) { node->update(offset); }
        draw_list->ChannelsMerge();

        // Update and draw links
        for (auto& l : m_links) { l.lock()->update(); }

        // Links drop-off
        if(m_dragOut && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if(!m_hovering)
            {
                if(on_free_space() && m_droppedLinkPopUp)
                {
                    if (m_droppedLinkPupUpComboKey == ImGuiKey_None || ImGui::IsKeyDown(m_droppedLinkPupUpComboKey))
                    {
                        m_droppedLinkLeft = m_dragOut;
                        ImGui::OpenPopup("DroppedLinkPopUp");
                    }
                }
                goto drop_off_end;
            }
            if (m_dragOut->parent() == m_hovering->parent())
                goto drop_off_end;
            if (!((m_dragOut->filter() & m_hovering->filter()) != 0 || m_dragOut->filter() == ConnectionFilter_None || m_hovering->filter() == ConnectionFilter_None)) // Check Filter
                goto drop_off_end;

            if (m_dragOut->kind() == PinKind_Output && m_hovering->kind() == PinKind_Input) // OUT to IN
            {
                if (m_hovering->getLink().expired() || m_hovering->getLink().lock()->left() != m_dragOut)
                    createLink(m_dragOut, m_hovering);
                else
                    m_hovering->deleteLink();
            }
            if (m_dragOut->kind() == PinKind_Input && m_hovering->kind() == PinKind_Output) // OUT to IN
            {
                if (m_dragOut->getLink().expired() || m_dragOut->getLink().lock()->left() != m_hovering)
                    createLink(m_hovering, m_dragOut);
                else
                    m_dragOut->deleteLink();
            }
        }
        drop_off_end:

        // Links drag-out
        if (!m_draggingNode && m_hovering && !m_dragOut && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            m_dragOut = m_hovering;
        if (m_dragOut)
        {
            ImVec2 pinDot;
            if (m_dragOut->kind() == PinKind_Output)
                smart_bezier(m_dragOut->pinPoint(), ImGui::GetMousePos(), m_style.colors.drag_out_link, m_style.drag_out_link_thickness);
            else
                smart_bezier(ImGui::GetMousePos(), m_dragOut->pinPoint(), m_style.colors.drag_out_link, m_style.drag_out_link_thickness);

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                m_dragOut = nullptr;
        }

        // Deletion of selected stuff
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            for (auto &l: m_links)
                if (l.lock()->selected())
                    l.lock()->right()->deleteLink();
            
            m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(),
                           [](const std::shared_ptr<BaseNode>& n) { return n->selected(); }), m_nodes.end());
        }

        // Removing dead references
        m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                       [](const std::weak_ptr<Link>& l) { return l.expired(); }), m_links.end());

        // Right-click PopUp
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && on_free_space())
        {
            if (m_rightClickPopUp)
                ImGui::OpenPopup("RightClickPopUp");
        }
        if (ImGui::BeginPopup("RightClickPopUp"))
        {
            m_rightClickPopUp();
            ImGui::EndPopup();
        }

        // Dropped Link PopUp
        if (ImGui::BeginPopup("DroppedLinkPopUp"))
        {
            m_droppedLinkPopUp(m_droppedLinkLeft);
            ImGui::EndPopup();
        }

        // Scrolling
        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
            m_scroll = m_scroll + ImGui::GetIO().MouseDelta;

        ImGui::EndChild();
    }
}
