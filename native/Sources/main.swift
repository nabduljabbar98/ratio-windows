import AppKit
import CoreGraphics

// Pure accounting: unknown, paused, and idle time never enter the ratio.
struct AppUsage: Codable {
    var name: String
    var seconds: Double = 0
    var unclassified: Double? = 0
    var createSeconds: Double?
    var consumeSeconds: Double?
}

struct Ledger: Codable {
    var day: String
    var consume: Double = 0
    var create: Double = 0
    var apps: [String: AppUsage]? = [:] // Optional preserves totals saved by v0.1.
    mutating func record(_ seconds: Double, mode: String?, appID: String = "", appName: String = "") {
        guard seconds > 0, seconds <= 3 else { return }
        if !appID.isEmpty {
            var usage = apps ?? [:]
            var entry = usage[appID] ?? AppUsage(name: appName)
            if entry.createSeconds == nil || entry.consumeSeconds == nil {
                let classified = max(0, entry.seconds - (entry.unclassified ?? 0))
                entry.createSeconds = mode == "create" ? classified : 0
                entry.consumeSeconds = mode == "consume" ? classified : 0
            }
            entry.name = appName; entry.seconds += seconds
            if mode == "create" { entry.createSeconds = (entry.createSeconds ?? 0) + seconds }
            if mode == "consume" { entry.consumeSeconds = (entry.consumeSeconds ?? 0) + seconds }
            if mode == nil { entry.unclassified = (entry.unclassified ?? 0) + seconds }
            usage[appID] = entry; apps = usage
        }
        if mode == "create" { create += seconds }
        if mode == "consume" { consume += seconds }
    }
    mutating func classifyPending(_ id: String, mode: String, previousMode: String? = nil) {
        guard mode == "create" || mode == "consume", var usage = apps?[id] else { return }
        let classified = max(0, usage.seconds - (usage.unclassified ?? 0))
        let oldCreate = usage.createSeconds ?? (previousMode == "create" ? classified : 0)
        let oldConsume = usage.consumeSeconds ?? (previousMode == "consume" ? classified : 0)
        create = max(0, create - oldCreate)
        consume = max(0, consume - oldConsume)
        usage.createSeconds = mode == "create" ? usage.seconds : 0
        usage.consumeSeconds = mode == "consume" ? usage.seconds : 0
        create += usage.createSeconds ?? 0
        consume += usage.consumeSeconds ?? 0
        usage.unclassified = 0; apps?[id] = usage
    }


}
func dayKey(_ date: Date = Date()) -> String {
    let f = DateFormatter(); f.dateFormat = "yyyy-MM-dd"; return f.string(from: date)
}

// One type scale throughout the interface; only the ratio is enlarged.
let interfaceFont = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
let panelBackground = NSColor(white: 0.06, alpha: 1)
let createColor = NSColor(srgbRed: 40/255, green: 205/255, blue: 65/255, alpha: 1)
let consumeColor = NSColor(srgbRed: 1, green: 59/255, blue: 48/255, alpha: 1)
let gridColor = NSColor(white: 0.14, alpha: 1)
func hairline(_ rect: NSRect) { gridColor.setFill(); NSBezierPath(rect: rect).fill() }

class GridButton: NSButton {
    var drawsGridEdges = true
    var drawsBottomEdge = true
    override func draw(_ dirtyRect: NSRect) {
        let selected = state == .on || isHighlighted
        (selected ? NSColor.white : panelBackground).setFill()
        NSBezierPath(rect: bounds).fill()
        let attrs: [NSAttributedString.Key: Any] = [.font: interfaceFont, .foregroundColor: selected ? NSColor.black : NSColor.white]
        let text = title.uppercased() as NSString
        let size = text.size(withAttributes: attrs)
        text.draw(at: NSPoint(x: (bounds.width - size.width) / 2, y: (bounds.height - size.height) / 2), withAttributes: attrs)
        let pixel = 1 / (window?.backingScaleFactor ?? 2)
        if drawsGridEdges {
            hairline(NSRect(x: bounds.width - pixel, y: 0, width: pixel, height: bounds.height))
            if drawsBottomEdge { hairline(NSRect(x: 0, y: 0, width: bounds.width, height: pixel)) }
        }
    }
}

final class AppListView: NSView {
    weak var owner: AppDelegate?
    override var isFlipped: Bool { true }
    override func draw(_ dirtyRect: NSRect) {
        panelBackground.setFill(); NSBezierPath(rect: bounds).fill()
        guard let owner = owner else { return }
        let rows = (owner.ledger.apps ?? [:]).sorted {
            $0.value.seconds == $1.value.seconds ? $0.key < $1.key : $0.value.seconds > $1.value.seconds
        }
        let total = rows.reduce(0) { $0 + $1.value.seconds }
        let attrs: [NSAttributedString.Key: Any] = [.font: interfaceFont, .foregroundColor: NSColor.white]
        if rows.isEmpty {
            ("APP USE WILL APPEAR HERE" as NSString).draw(at: NSPoint(x: 16, y: 20), withAttributes: attrs)
        }
        let pixel = 1 / (window?.backingScaleFactor ?? 2)
        for (index, row) in rows.enumerated() {
            let y = CGFloat(index * 56)
            let name = row.value.name + (row.key == owner.activeID ? " ·" : "")
            let style = NSMutableParagraphStyle(); style.lineBreakMode = .byTruncatingTail
            var nameAttrs = attrs; nameAttrs[.paragraphStyle] = style
            (name as NSString).draw(in: NSRect(x: 16, y: y + 12, width: 230, height: 18), withAttributes: nameAttrs)
            let duration = owner.duration(row.value.seconds) as NSString
            duration.draw(at: NSPoint(x: 344 - duration.size(withAttributes: attrs).width, y: y + 12), withAttributes: attrs)
            NSColor.white.setFill(); NSBezierPath(rect: NSRect(x: 16, y: y + 39, width: total > 0 ? 232 * min(1, max(0, row.value.seconds / total)) : 0, height: pixel)).fill()
            hairline(NSRect(x: 264, y: y, width: pixel, height: 56))
            hairline(NSRect(x: 0, y: y + 56 - pixel, width: bounds.width, height: pixel))
        }
    }
}

final class ReviewButton: GridButton {
    var siteID = ""
    var mode = ""
    override func draw(_ dirtyRect: NSRect) {
        (state == .on || isHighlighted ? NSColor(white: 0.075, alpha: 1) : panelBackground).setFill()
        NSBezierPath(rect: bounds).fill()
        let color: NSColor = mode == "create" ? createColor : consumeColor
        let attrs: [NSAttributedString.Key: Any] = [.font: interfaceFont, .foregroundColor: color]
        let text = title as NSString; let size = text.size(withAttributes: attrs)
        text.draw(at: NSPoint(x: (bounds.width - size.width) / 2, y: (bounds.height - size.height) / 2), withAttributes: attrs)
        // Row separators belong to the list; each control owns only its left edge.
        let pixel = 1 / (window?.backingScaleFactor ?? 2)
        hairline(NSRect(x: 0, y: 0, width: pixel, height: bounds.height))
    }
}
final class ReviewListView: NSView {
    override var isFlipped: Bool { true }
}

final class CaretDividerView: NSView {
    weak var panel: NSView?
    override func hitTest(_ point: NSPoint) -> NSView? { nil }
    override func draw(_ dirtyRect: NSRect) {
        guard let panel = panel else { return }
        let content = convert(panel.bounds, from: panel)
        let pixel = 1 / (window?.backingScaleFactor ?? 2)
        let bottom = content.maxY
        let height: CGFloat = 9
        let tip = bottom + height - pixel
        let edge = content.insetBy(dx: pixel / 2, dy: pixel / 2)
        let radius: CGFloat = 10
        let tangent = radius * (1 - 0.5522847498)
        let x = content.midX
        let outline = NSBezierPath()
        outline.move(to: NSPoint(x: x - height, y: edge.maxY))
        outline.line(to: NSPoint(x: edge.minX + radius, y: edge.maxY))
        outline.curve(to: NSPoint(x: edge.minX, y: edge.maxY - radius), controlPoint1: NSPoint(x: edge.minX + tangent, y: edge.maxY), controlPoint2: NSPoint(x: edge.minX, y: edge.maxY - tangent))
        outline.line(to: NSPoint(x: edge.minX, y: edge.minY + radius))
        outline.curve(to: NSPoint(x: edge.minX + radius, y: edge.minY), controlPoint1: NSPoint(x: edge.minX, y: edge.minY + tangent), controlPoint2: NSPoint(x: edge.minX + tangent, y: edge.minY))
        outline.line(to: NSPoint(x: edge.maxX - radius, y: edge.minY))
        outline.curve(to: NSPoint(x: edge.maxX, y: edge.minY + radius), controlPoint1: NSPoint(x: edge.maxX - tangent, y: edge.minY), controlPoint2: NSPoint(x: edge.maxX, y: edge.minY + tangent))
        outline.line(to: NSPoint(x: edge.maxX, y: edge.maxY - radius))
        outline.curve(to: NSPoint(x: edge.maxX - radius, y: edge.maxY), controlPoint1: NSPoint(x: edge.maxX, y: edge.maxY - tangent), controlPoint2: NSPoint(x: edge.maxX - tangent, y: edge.maxY))
        outline.line(to: NSPoint(x: x + height, y: edge.maxY))
        outline.curve(to: NSPoint(x: x, y: tip),
                      controlPoint1: NSPoint(x: x + height * 0.55, y: edge.maxY),
                      controlPoint2: NSPoint(x: x + height * 0.45, y: tip))
        outline.curve(to: NSPoint(x: x - height, y: edge.maxY),
                      controlPoint1: NSPoint(x: x - height * 0.45, y: tip),
                      controlPoint2: NSPoint(x: x - height * 0.55, y: edge.maxY))
        outline.close()
        // Fill only the caret area from the exact outline used for its stroke.
        NSGraphicsContext.saveGraphicsState()
        NSBezierPath(rect: NSRect(x: content.minX, y: bottom, width: content.width, height: height + 1)).addClip()
        panelBackground.setFill(); outline.fill()
        NSGraphicsContext.restoreGraphicsState()
        gridColor.setStroke(); outline.lineWidth = pixel; outline.stroke()
    }
}

final class RatioView: NSView {
    private var caretDivider: CaretDividerView?
    weak var owner: AppDelegate?
    var selectedTab = 0
    var reviewingPending = false
    let notifications = GridButton(title: "", target: nil, action: nil)
    let ratioTab = GridButton(title: "Ratio", target: nil, action: nil)
    let appsTab = GridButton(title: "Apps", target: nil, action: nil)
    let appScroll = NSScrollView()
    let appList = AppListView(frame: .zero)
    let reviewList = ReviewListView(frame: .zero)
    let reviewScroll = NSScrollView()
    let reviewButton = GridButton(title: "Review sites", target: nil, action: nil)
    var reviewSignature = ""
    var showingApps: Bool { selectedTab == 1 }
    let title = NSTextField(labelWithString: "TODAY")
    let totals = NSTextField(labelWithString: "")
    let context = NSTextField(wrappingLabelWithString: "")
    let trackedTotal = NSTextField(labelWithString: "")
    let note = NSTextField(wrappingLabelWithString: "")
    let consume = GridButton(title: "↓ Consume", target: nil, action: #selector(AppDelegate.chooseConsume))
    let create = GridButton(title: "↑ Create", target: nil, action: #selector(AppDelegate.chooseCreate))
    let pause = GridButton(title: "Pause", target: nil, action: #selector(AppDelegate.togglePause))
    let forget = GridButton(title: "Reset", target: nil, action: #selector(AppDelegate.resetAll))
    let quit = GridButton(title: "Quit", target: NSApp, action: #selector(NSApplication.terminate(_:)))
    override init(frame: NSRect) {
        super.init(frame: frame)
        wantsLayer = true; layer?.backgroundColor = panelBackground.cgColor
        for button in [ratioTab, appsTab, consume, create, pause, forget, quit] {
            button.font = interfaceFont; button.isBordered = false; button.setButtonType(.momentaryPushIn); addSubview(button)
        }
        ratioTab.target = self; ratioTab.action = #selector(showRatio)
        appsTab.target = self; appsTab.action = #selector(showApps)
        ratioTab.frame = NSRect(x: 0, y: 396, width: 180, height: 44)
        appsTab.frame = NSRect(x: 180, y: 396, width: 180, height: 44)
        for label in [title, totals, context, note] { label.font = interfaceFont; label.textColor = .white; addSubview(label) }
        totals.font = .monospacedSystemFont(ofSize: 48, weight: .regular)
        totals.alignment = .center
        title.frame = NSRect(x: 16, y: 365, width: 260, height: 18)
        let brand = NSTextField(labelWithString: "RATIO")
        brand.isHidden = true; title.isHidden = true
        brand.font = interfaceFont; brand.textColor = .gray; brand.alignment = .right
        brand.frame = NSRect(x: 276, y: 365, width: 68, height: 18); addSubview(brand)
        notifications.frame = NSRect(x: 273, y: 265, width: 87, height: 42)
        notifications.drawsGridEdges = false
        notifications.target = self; notifications.action = #selector(togglePending)
        notifications.font = interfaceFont; notifications.isBordered = false
        addSubview(notifications)
        totals.frame = NSRect(x: 16, y: 269, width: 328, height: 65)
        context.frame = NSRect(x: 16, y: 277, width: 156, height: 18)
        context.textColor = .gray
        trackedTotal.font = interfaceFont; trackedTotal.textColor = .gray; trackedTotal.alignment = .right
        trackedTotal.frame = NSRect(x: 180, y: 277, width: 84, height: 18)
        addSubview(trackedTotal)
        note.frame = NSRect(x: 16, y: 53, width: 328, height: 46)
        consume.frame = NSRect(x: 180, y: 108, width: 180, height: 44)
        create.frame = NSRect(x: 0, y: 108, width: 180, height: 44)
        pause.frame = NSRect(x: 0, y: 0, width: 120, height: 44)
        forget.frame = NSRect(x: 120, y: 0, width: 120, height: 44)
        quit.frame = NSRect(x: 240, y: 0, width: 120, height: 44)
        pause.drawsBottomEdge = false; forget.drawsBottomEdge = false
        quit.drawsGridEdges = false
        appScroll.frame = NSRect(x: 0, y: 108, width: 360, height: 244)
        appScroll.drawsBackground = false; appScroll.hasVerticalScroller = true; appScroll.scrollerStyle = .overlay
        appScroll.documentView = appList; addSubview(appScroll); appScroll.isHidden = true
        reviewScroll.frame = NSRect(x: 0, y: 44, width: 360, height: 220); reviewScroll.drawsBackground = false
        reviewScroll.hasVerticalScroller = true; reviewScroll.scrollerStyle = .overlay
        reviewScroll.documentView = reviewList; addSubview(reviewScroll); reviewScroll.isHidden = true
        reviewButton.frame = NSRect(x: 0, y: 44, width: 360, height: 64)
        reviewButton.target = self; reviewButton.action = #selector(showReview); addSubview(reviewButton)
        reviewButton.isHidden = true
    }
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        // The native popover draws its arrow outside our content view.
        // Color that backing surface too, so the arrow matches the panel.
        window?.backgroundColor = panelBackground
        window?.hasShadow = false
        var ancestor = superview
        while let view = ancestor {
            if let effect = view as? NSVisualEffectView {
                effect.state = .inactive
                effect.wantsLayer = true
                effect.layer?.backgroundColor = panelBackground.cgColor
            }
            ancestor = view.superview
        }
        DispatchQueue.main.async { [weak self] in
            guard let self = self, self.window != nil else { return }
            self.window?.hasShadow = false
            self.window?.invalidateShadow()
            var surface: NSView = self
            while let parent = surface.superview { surface = parent }
            self.caretDivider?.removeFromSuperview()
            let divider = CaretDividerView(frame: surface.bounds)
            divider.panel = self
            divider.autoresizingMask = [.width, .height]
            surface.addSubview(divider, positioned: .above, relativeTo: nil)
            self.caretDivider = divider
        }
    }
    @objc func togglePending() {
        reviewingPending.toggle()
        reviewScroll.contentView.scroll(to: .zero)
        owner?.render()
    }
    @objc func showReview() { selectedTab = 2; owner?.render() }
    @objc func showRatio() { selectedTab = 0; owner?.render() }
    @objc func showApps() { selectedTab = 1; owner?.render() }
    func refreshApps() {
        selectedTab = 0
        ratioTab.isHidden = true; appsTab.isHidden = true
        totals.isHidden = true; context.isHidden = false
        consume.isHidden = true; create.isHidden = true
        appScroll.isHidden = true; reviewScroll.isHidden = false
        reviewButton.isHidden = true; note.isHidden = true
        let count = owner?.pendingSites.count ?? 0
        notifications.title = count > 0 ? "! \(count)" : "✓"
        notifications.state = reviewingPending ? .on : .off
        notifications.toolTip = reviewingPending ? "Show all activity" : "\(count) items need categorizing"
        notifications.setAccessibilityLabel(notifications.toolTip)
        notifications.needsDisplay = true
        let pending = (owner?.ledger.apps ?? [:]).filter { !reviewingPending || ($0.value.unclassified ?? 0) >= 1 }.sorted {
            $0.value.seconds == $1.value.seconds ? $0.key < $1.key : $0.value.seconds > $1.value.seconds
        }
        func selectedMode(_ id: String) -> String? {
            guard let owner = owner else { return nil }
            if id == owner.activeID { return owner.mode }
            return owner.rules[id] ?? owner.builtIns[id] ?? (id.hasPrefix("site:") ? owner.siteMode(String(id.dropFirst(5))) : nil)
        }
        let signature = pending.map { $0.key + ":" + (selectedMode($0.key) ?? "?") }.joined(separator: "|") + (owner?.activeID ?? "") + String(reviewingPending)
        if signature != reviewSignature {
            reviewSignature = signature
            reviewList.subviews.forEach { $0.removeFromSuperview() }
            if pending.isEmpty {
                let empty = NSTextField(labelWithString: reviewingPending ? "All caught up." : "Activity will appear here.")
                empty.font = interfaceFont; empty.textColor = .white
                empty.frame = NSRect(x: 16, y: 13, width: 328, height: 18)
                reviewList.addSubview(empty)
            }
            for (i, row) in pending.enumerated() {
                let y = CGFloat(i * 44)
                let label = NSTextField(labelWithString: row.value.name)
                label.font = interfaceFont; label.textColor = .white
                label.lineBreakMode = .byTruncatingTail
                label.frame = NSRect(x: 16, y: y + 13, width: 172, height: 18)
                reviewList.addSubview(label)
                let time = NSTextField(labelWithString: owner?.duration(row.value.seconds) ?? "")
                time.identifier = NSUserInterfaceItemIdentifier(row.key)
                time.font = interfaceFont; time.textColor = .white; time.alignment = .right
                time.frame = NSRect(x: 180, y: y + 13, width: 84, height: 18); reviewList.addSubview(time)
                for (j, mode) in ["create", "consume"].enumerated() {
                    let button = ReviewButton(title: mode == "consume" ? "↓" : "↑", target: owner, action: #selector(AppDelegate.reviewSite(_:)))
                    button.siteID = row.key; button.mode = mode; button.font = interfaceFont
                    button.state = selectedMode(row.key) == mode ? .on : .off
                    button.isBordered = false; button.contentTintColor = .white
                    button.toolTip = mode == "create" ? "Create" : "Consume"
                    button.setAccessibilityLabel((mode == "create" ? "Create: " : "Consume: ") + row.value.name)
                    button.frame = NSRect(x: 272 + CGFloat(j * 44), y: y, width: 44, height: 44)
                    reviewList.addSubview(button)
                }
                let pixel = 1 / (window?.backingScaleFactor ?? 2)
                let line = NSView(frame: NSRect(x: 0, y: y + 44 - pixel, width: 360, height: pixel))
                line.wantsLayer = true; line.layer?.backgroundColor = gridColor.cgColor; reviewList.addSubview(line)
            }
            reviewList.setFrameSize(NSSize(width: 360, height: max(220, pending.count * 44)))
        }
        for case let time as NSTextField in reviewList.subviews {
            if let id = time.identifier?.rawValue, let usage = owner?.ledger.apps?[id] {
                let active = id == owner?.activeID && owner?.paused == false && owner?.idle == false && owner?.sleeping == false
                let text = (active ? "● " : "") + (owner?.duration(usage.seconds) ?? "")
                let value = NSMutableAttributedString(string: text, attributes: [.font: interfaceFont, .foregroundColor: NSColor.white])
                if active {
                    value.addAttributes([.foregroundColor: createColor, .font: NSFont.monospacedSystemFont(ofSize: 7, weight: .regular), .baselineOffset: 2], range: NSRange(location: 0, length: 1))
                }
                let alignment = NSMutableParagraphStyle(); alignment.alignment = .right
                value.addAttribute(.paragraphStyle, value: alignment, range: NSRange(location: 0, length: value.length))
                time.attributedStringValue = value
            }
        }
        ratioTab.state = selectedTab == 0 ? .on : .off; appsTab.state = selectedTab == 0 ? .off : .on
        let height = max(244, (owner?.ledger.apps?.count ?? 0) * 56)
        appList.setFrameSize(NSSize(width: 360, height: height)); appList.needsDisplay = true
        for button in [ratioTab, appsTab, consume, create, pause, forget, quit] { button.needsDisplay = true }
    }
    required init?(coder: NSCoder) { fatalError() }
    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        let pixel = 1 / (window?.backingScaleFactor ?? 2)
        for y: CGFloat in [44, 264, 352] { hairline(NSRect(x: 0, y: y, width: 360, height: pixel)) }
        // Dividers are inset one physical pixel into adjacent cells to remain visible.
        hairline(NSRect(x: 272, y: 264, width: pixel, height: 44))
        guard let o = owner, selectedTab == 0 else { return }
        for y: CGFloat in [308, 352] { hairline(NSRect(x: 0, y: y, width: 360, height: pixel)) }

        let total = o.ledger.consume + o.ledger.create
        let fraction = total > 0 ? o.ledger.create / total : 0.5
        consumeColor.setFill(); NSBezierPath(rect: NSRect(x: 0, y: 308, width: 360, height: pixel)).fill()
        createColor.setFill(); NSBezierPath(rect: NSRect(x: 0, y: 308, width: 360 * fraction, height: pixel)).fill()
        let attrs: [NSAttributedString.Key: Any] = [.font: interfaceFont, .foregroundColor: NSColor.white]
        let consumeText = NSMutableAttributedString(string: "↓ " + o.duration(o.ledger.consume) + " CONSUMING", attributes: attrs)
        consumeText.addAttribute(.foregroundColor, value: consumeColor, range: NSRange(location: 0, length: 1))
        consumeText.draw(at: NSPoint(x: 196, y: 323))
        let createText = NSMutableAttributedString(string: "↑ " + o.duration(o.ledger.create) + " CREATING", attributes: attrs)
        createText.addAttribute(.foregroundColor, value: createColor, range: NSRange(location: 0, length: 1))
        createText.draw(at: NSPoint(x: 16, y: 323))
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    var ledger = Ledger(day: dayKey())
    var rules: [String: String] = [:]
    var status: NSStatusItem!
    let popover = NSPopover()
    var outsideClickMonitor: Any?
    var localClickMonitor: Any?
    var panel: RatioView!
    var timer: Timer?
    var activeID = ""
    var activeName = "No active app"
    var browserID: String?
    var browserName = ""
    var checkingBrowser = false
    let browsers = ["com.apple.Safari", "com.google.Chrome", "com.brave.Browser", "com.microsoft.edgemac", "company.thebrowser.Browser"]
    let consumeSites = ["x.com", "twitter.com", "youtube.com", "reddit.com", "instagram.com", "tiktok.com", "netflix.com"]
    let createSites = ["figma.com", "docs.google.com", "canva.com"]
    var mode: String?
    var switched = Date()
    var lastTick = Date()
    var lastPrompt = Date.distantPast
    var prompted = Set<String>()
    var paused = false
    var sleeping = false
    var idle = false
    var ticks = 0
    var activeSeconds: Double = 0
    var chromeSessionSites = Set<String>()
    var reviewWork: DispatchWorkItem?
    var pendingSites: [(key: String, value: AppUsage)] {
        (ledger.apps ?? [:]).filter { ($0.value.unclassified ?? 0) >= 1 }.sorted { $0.value.seconds > $1.value.seconds }
    }
    @objc func reviewSite(_ button: ReviewButton) {
        tick()
        let previous = rules[button.siteID] ?? builtIns[button.siteID] ?? (button.siteID.hasPrefix("site:") ? siteMode(String(button.siteID.dropFirst(5))) : nil)
        ledger.classifyPending(button.siteID, mode: button.mode, previousMode: previous)
        rules[button.siteID] = button.mode
        if activeID == button.siteID { mode = button.mode }
        if pendingSites.isEmpty { panel.selectedTab = 1 }
        save(); render()
    }
    let defaults = UserDefaults.standard
    let builtIns: [String: String] = [
        "com.apple.dt.Xcode": "create", "com.microsoft.VSCode": "create", "com.todesktop.230313mzl4w4u92": "create",
        "com.figma.Desktop": "create", "com.adobe.Photoshop": "create", "com.adobe.Illustrator": "create",
        "com.apple.iWork.Pages": "create", "com.apple.iWork.Keynote": "create", "com.apple.iWork.Numbers": "create",
        "com.apple.FinalCut": "create", "com.apple.garageband10": "create", "com.apple.Logic10": "create",
        "com.apple.TV": "consume", "com.apple.iBooksX": "consume", "com.apple.news": "consume"
    ]
    func applicationDidFinishLaunching(_ notification: Notification) {
        if let data = defaults.data(forKey: "ledger"), let saved = try? JSONDecoder().decode(Ledger.self, from: data) { ledger = saved }
        rules = defaults.dictionary(forKey: "rules") as? [String: String] ?? [:]
        // Migrate existing per-app history to explicit category contributions.
        if let history = ledger.apps, history.values.contains(where: { $0.createSeconds == nil || $0.consumeSeconds == nil }) {
            let attributed = history.values.reduce(0) { $0 + max(0, $1.seconds - ($1.unclassified ?? 0)) }
            let legacy = max(0, ledger.create + ledger.consume - attributed)
            let fraction = ledger.create + ledger.consume > 0 ? ledger.create / (ledger.create + ledger.consume) : 0
            ledger.create = legacy * fraction; ledger.consume = legacy * (1 - fraction)
            for (id, var usage) in history {
                let category = rules[id] ?? builtIns[id] ?? (id.hasPrefix("site:") ? siteMode(String(id.dropFirst(5))) : nil)
                let classified = max(0, usage.seconds - (usage.unclassified ?? 0))
                usage.createSeconds = usage.createSeconds ?? (category == "create" ? classified : 0)
                usage.consumeSeconds = usage.consumeSeconds ?? (category == "consume" ? classified : 0)
                ledger.create += usage.createSeconds ?? 0
                ledger.consume += usage.consumeSeconds ?? 0
                ledger.apps?[id] = usage
            }
        }

        rollover()
        status = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        status.button?.target = self; status.button?.action = #selector(togglePopover)
        status.button?.font = interfaceFont
        let controller = NSViewController()
        panel = RatioView(frame: NSRect(x: 0, y: 0, width: 360, height: 352)); panel.owner = self
        for button in [panel.consume, panel.create, panel.pause, panel.forget] { button.target = self }
        controller.view = panel; popover.contentViewController = controller; popover.behavior = .transient
        popover.appearance = NSAppearance(named: .darkAqua)
        let clicks: NSEvent.EventTypeMask = [.leftMouseDown, .rightMouseDown, .otherMouseDown]
        outsideClickMonitor = NSEvent.addGlobalMonitorForEvents(matching: clicks) { [weak self] _ in
            guard let self = self, self.popover.isShown else { return }
            self.popover.performClose(nil)
        }
        localClickMonitor = NSEvent.addLocalMonitorForEvents(matching: clicks) { [weak self] event in
            guard let self = self, self.popover.isShown else { return event }
            if event.window !== self.panel.window && event.window !== self.status.button?.window {
                self.popover.performClose(nil)
            }
            return event
        }

        let nc = NSWorkspace.shared.notificationCenter
        nc.addObserver(self, selector: #selector(activated(_:)), name: NSWorkspace.didActivateApplicationNotification, object: nil)
        nc.addObserver(self, selector: #selector(sleepNow), name: NSWorkspace.willSleepNotification, object: nil)
        nc.addObserver(self, selector: #selector(sleepNow), name: NSWorkspace.sessionDidResignActiveNotification, object: nil)
        nc.addObserver(self, selector: #selector(wakeNow), name: NSWorkspace.didWakeNotification, object: nil)
        nc.addObserver(self, selector: #selector(wakeNow), name: NSWorkspace.sessionDidBecomeActiveNotification, object: nil)
        updateApp(NSWorkspace.shared.frontmostApplication)
        timer = Timer(timeInterval: 1, repeats: true) { [weak self] _ in self?.tick() }
        RunLoop.main.add(timer!, forMode: .common)
        render(); showPopover(); checkBrowser()
    }
    func rollover() {
        let today = dayKey()
        if ledger.day != today { save(); ledger = Ledger(day: today) }
    }
    func save() {
        if let data = try? JSONEncoder().encode(ledger) { defaults.set(data, forKey: "ledger") }
        defaults.set(rules, forKey: "rules")
    }
    func tick() {
        let now = Date(); let elapsed = now.timeIntervalSince(lastTick); lastTick = now
        let oldDay = ledger.day; rollover()
        idle = CGEventSource.secondsSinceLastEventType(.combinedSessionState, eventType: CGEventType(rawValue: UInt32.max)!) >= 60
        if !paused && !sleeping && !idle && oldDay == ledger.day {
            ledger.record(elapsed, mode: mode, appID: activeID, appName: activeName)
            if browserID == "com.google.Chrome" && activeID.hasPrefix("site:") && mode == nil { chromeSessionSites.insert(activeID) }
            if elapsed > 0 && elapsed <= 3 && !activeID.isEmpty { activeSeconds += elapsed }
        }
        ticks += 1; if ticks % 10 == 0 { save() }
        if ticks % 3 == 0 && !sleeping && !paused { checkBrowser() }
        render()
    }
    @objc func activated(_ n: Notification) {
        guard let next = n.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication,
              next.processIdentifier != ProcessInfo.processInfo.processIdentifier else { return }
        tick()
        updateApp(next)
        render(); checkBrowser()
    }
    func updateApp(_ app: NSRunningApplication?) {
        guard let app = app, app.processIdentifier != ProcessInfo.processInfo.processIdentifier else { return }
        activeSeconds = 0
        activeID = app.bundleIdentifier ?? "process:\(app.localizedName ?? "unknown")"
        activeName = app.localizedName ?? "Unknown app"
        browserID = browsers.contains(activeID) ? activeID : nil
        browserName = activeName
        mode = rules[activeID] ?? builtIns[activeID]; switched = Date(); lastTick = Date()
    }
    func siteMode(_ host: String) -> String? {
        if consumeSites.contains(where: { host == $0 || host.hasSuffix("." + $0) }) { return "consume" }
        if createSites.contains(where: { host == $0 || host.hasSuffix("." + $0) }) { return "create" }
        return nil
    }
    func checkBrowser() {
        guard let id = browserID, !checkingBrowser else { return }
        checkingBrowser = true
        // Read only the active tab URL. Store the hostname, never paths or queries.
        let command = id == "com.apple.Safari" ? "get URL of current tab of front window" : "get URL of active tab of front window"
        let source = "tell application id \"" + id + "\" to " + command
        DispatchQueue.global(qos: .utility).async { [weak self] in
            var error: NSDictionary?
            let value = NSAppleScript(source: source)?.executeAndReturnError(&error).stringValue
            let host = value.flatMap { URL(string: $0)?.host?.lowercased() }.map { $0.hasPrefix("www.") ? String($0.dropFirst(4)) : $0 }
            DispatchQueue.main.async {
                guard let self = self else { return }
                self.checkingBrowser = false
                guard self.browserID == id else { return }
                let key = host.map { "site:" + $0 } ?? id
                guard key != self.activeID else { return }
                // Settle the previous context before applying the new website.
                self.tick()
                self.activeSeconds = 0
                self.activeID = key
                self.activeName = host ?? self.browserName
                self.mode = self.rules[key] ?? host.flatMap { self.siteMode($0) }
                self.switched = Date(); self.lastTick = Date(); self.render()
            }
        }
    }
    @objc func sleepNow() { tick(); sleeping = true; save(); render() }
    @objc func wakeNow() { sleeping = false; lastTick = Date(); updateApp(NSWorkspace.shared.frontmostApplication); render() }
    @objc func chooseConsume() { choose("consume") }
    @objc func chooseCreate() { choose("create") }
    func choose(_ value: String) {
        guard !activeID.isEmpty else { return }
        tick(); mode = value; rules[activeID] = value; save(); render()
    }
    @objc func resetAll() {
        ledger = Ledger(day: dayKey())
        rules = [:]; activeSeconds = 0; paused = false
        prompted.removeAll(); chromeSessionSites.removeAll()
        panel.reviewingPending = false
        mode = builtIns[activeID] ?? (activeID.hasPrefix("site:") ? siteMode(String(activeID.dropFirst(5))) : nil)
        lastTick = Date(); save(); render()
    }
    @objc func forgetApp() { tick(); rules.removeValue(forKey: activeID); mode = nil; prompted.insert(activeID); save(); render() }
    @objc func togglePause() { tick(); paused.toggle(); lastTick = Date(); save(); render() }
    @objc func togglePopover() { if popover.isShown { popover.performClose(nil) } else { showPopover() } }
    func showPopover() {
        guard let button = status.button, !popover.isShown else { return }
        popover.show(relativeTo: button.bounds, of: button, preferredEdge: .minY)
    }
    func duration(_ seconds: Double) -> String {
        let s = max(0, Int(seconds))
        return s >= 3600 ? String(format: "%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60) : String(format: "%d:%02d", s / 60, s % 60)
    }
    func render() {
        panel.selectedTab = 0
        let total = ledger.consume + ledger.create
        let c = total > 0 ? Int((ledger.create / total * 100).rounded()) : 0
        let ratioText = total > 0 ? "\(c) / \(100 - c)" : "— / —"
        let styledRatio = NSMutableAttributedString(string: ratioText, attributes: [.font: NSFont.monospacedSystemFont(ofSize: 48, weight: .regular), .foregroundColor: NSColor.white])
        if let slash = ratioText.range(of: "/ ") {
            let range = NSRange(slash.upperBound..<ratioText.endIndex, in: ratioText)
            styledRatio.addAttribute(.foregroundColor, value: NSColor.darkGray, range: range)
        }
        let alignment = NSMutableParagraphStyle(); alignment.alignment = .center
        styledRatio.addAttribute(.paragraphStyle, value: alignment, range: NSRange(location: 0, length: styledRatio.length))
        panel.totals.attributedStringValue = styledRatio
        let state = paused ? "PAUSED" : sleeping || idle ? "AWAY" : mode == "create" ? "CREATING" : mode == "consume" ? "CONSUMING" : "UNCLASSIFIED"
        panel.title.stringValue = "CREATE:CONSUME"
        let tracking = !paused && !sleeping && !idle
        let liveStatus = tracking ? "TRACKING" : state
        panel.context.stringValue = liveStatus
        panel.trackedTotal.stringValue = duration((ledger.apps ?? [:]).values.reduce(0) { $0 + $1.seconds })
        panel.note.stringValue = paused ? "Tracking paused. Click Resume to count." : sleeping || idle ? "Away · counting resumes with activity." : mode == nil ? "App time is counting. Choose a mode to include it in your ratio." : state + " · time updates every second.\nClick a mode to correct it."
        panel.pause.title = paused ? "Resume" : "Pause"
        panel.consume.state = mode == "consume" ? .on : .off; panel.create.state = mode == "create" ? .on : .off
        let symbol = paused || idle || sleeping ? "Ⅱ" : mode == "create" ? "↑" : mode == "consume" ? "↓" : "?"
        let statusTitle = total > 0 ? "\(symbol) \(c)/\(100-c)" : "\(symbol) Ratio"
        let statusColor: NSColor = !tracking || mode == nil ? .labelColor : mode == "create" ? createColor : consumeColor
        status.button?.attributedTitle = NSAttributedString(string: statusTitle, attributes: [.font: interfaceFont, .foregroundColor: statusColor])
        status.button?.toolTip = "Ratio · \(state.lowercased()) · \(activeName)"
        if panel.showingApps {
            let appTotal = (ledger.apps ?? [:]).values.reduce(0) { $0 + $1.seconds }
            panel.title.stringValue = (tracking ? "TRACKING · " : state + " · ") + duration(appTotal) + " TODAY"
            panel.note.stringValue = "App + website time. Includes unclassified use. Pauses after 60s idle."
        }
        if panel.selectedTab == 2 {
            panel.title.stringValue = "CATEGORIZE / ACTIVITY"
            panel.note.stringValue = "Applies to today's uncategorized time.\nUse Ratio or Apps to review later."
        }
        if panel.reviewingPending { panel.context.stringValue = "TO CATEGORIZE" }
        panel.refreshApps()
        panel.needsDisplay = true
    }
    func applicationWillTerminate(_ notification: Notification) { save() }
}

if CommandLine.arguments.contains("--preview") {
    _ = NSApplication.shared
    let owner = AppDelegate()
    owner.ledger = Ledger(day: dayKey(), consume: 3600, create: 1200)
    owner.ledger.apps = ["site:x.com": AppUsage(name: "x.com", seconds: 3600), "editor": AppUsage(name: "Xcode", seconds: 1200)]
    owner.activeName = "x.com"; owner.activeID = "site:x.com"; owner.mode = "consume"
    owner.status = NSStatusBar.system.statusItem(withLength: 0)
    let view = RatioView(frame: NSRect(x: 0, y: 0, width: 360, height: 352))
    owner.panel = view; view.owner = owner
    let window = NSWindow(contentRect: view.bounds, styleMask: .borderless, backing: .buffered, defer: false)
    window.contentView = view
    owner.ledger.apps?["site:example.org"] = AppUsage(name: "example.org", seconds: 123, unclassified: 123)
    for tab in 0...2 {
        view.selectedTab = tab; owner.render(); view.display()
        let bitmap = view.bitmapImageRepForCachingDisplay(in: view.bounds)!
        view.cacheDisplay(in: view.bounds, to: bitmap)
        try! bitmap.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: CommandLine.arguments.last! + "/ratio-preview-\(tab).png"))
    }
} else if CommandLine.arguments.contains("--self-test") {
    let classifier = AppDelegate()
    precondition(classifier.siteMode("x.com") == "consume")
    precondition(classifier.siteMode("www.youtube.com") == "consume")
    precondition(classifier.siteMode("docs.google.com") == "create")
    precondition(classifier.siteMode("notx.com") == nil)
    precondition(classifier.siteMode("x.com.example.org") == nil)
    print("PASS: website classification and hostname boundaries")
    var l = Ledger(day: "test")
    l.record(2, mode: "create"); l.record(1, mode: "consume"); l.record(2, mode: nil)
    l.record(100, mode: "create"); l.record(-1, mode: "consume")
    precondition(l.create == 2 && l.consume == 1, "Accounting must exclude gaps and unknown time")
    let data = try! JSONEncoder().encode(l)
    let restored = try! JSONDecoder().decode(Ledger.self, from: data)
    precondition(restored.create == 2 && restored.day == "test")
    let legacy = try! JSONDecoder().decode(Ledger.self, from: Data("{\"day\":\"test\",\"consume\":7,\"create\":9}".utf8))
    precondition(legacy.consume == 7 && legacy.create == 9 && legacy.apps == nil)
    var usage = legacy
    usage.record(2, mode: nil, appID: "browser", appName: "Browser")
    usage.record(1, mode: "create", appID: "editor", appName: "Editor")
    usage.record(100, mode: "create", appID: "editor", appName: "Editor")
    precondition(usage.apps?["browser"]?.seconds == 2 && usage.apps?["editor"]?.seconds == 1)
    precondition(usage.consume == 7 && usage.create == 10)
    let usageSaved = try! JSONDecoder().decode(Ledger.self, from: JSONEncoder().encode(usage))
    precondition(usageSaved.apps?["browser"]?.name == "Browser")
    precondition(Ledger(day: "tomorrow").apps?.isEmpty == true)
    usage.classifyPending("browser", mode: "consume")
    precondition(usage.consume == 9 && usage.apps?["browser"]?.unclassified == 0)
    usage.classifyPending("browser", mode: "consume")
    precondition(usage.consume == 9, "Review must not double count")
    print("PASS: retrospective categorization and duplicate review protection")
    print("PASS: app attribution, legacy migration, daily reset, app persistence")
    var reclassified = Ledger(day: "test")
    reclassified.record(3, mode: "create", appID: "editor", appName: "Editor")
    reclassified.record(2, mode: "consume", appID: "social", appName: "Social")
    reclassified.classifyPending("social", mode: "create")
    precondition(reclassified.create == 5 && reclassified.consume == 0)
    reclassified.classifyPending("editor", mode: "consume")
    reclassified.classifyPending("social", mode: "consume")
    precondition(reclassified.create == 0 && reclassified.consume == 5)
    reclassified.classifyPending("social", mode: "consume")
    precondition(reclassified.consume == 5)
    let roundtrip = try! JSONDecoder().decode(Ledger.self, from: JSONEncoder().encode(reclassified))
    precondition(roundtrip.apps?["social"]?.consumeSeconds == 2)
    print("PASS: recategorization moves all app time, all-create/all-consume, idempotency and persistence")
    print("PASS: classified time, unknown exclusion, suspension gaps, persistence")
} else {
    let app = NSApplication.shared
    let delegate = AppDelegate(); app.delegate = delegate
    app.setActivationPolicy(.accessory); app.run()
}
