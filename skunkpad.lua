
-- mouse handler

focus = nil
l_button = false

function hitTest(frames, x, y)
	for i,f in ipairs(frames) do
		if f.shown then
			-- hit test children first
			local lx,ly = x-f.x, y-f.y
			local c,hx,hy = hitTest(f, lx, ly)
			if c then
				return c,hx,hy
			end
			-- hit test this frame
			if f.hit then
				if lx >= 0 and ly >= 0 and lx < f.width and ly < f.height then
					return f, lx, ly
				end
			end
		end
	end
end

function notify_pointer(x, y, b)
	local f,hx,hy = hitTest(frames, x, y)
	local lb = (b % 2 ~= 0)
	if f ~= focus then
		if focus then
			if focus.leave then focus:leave(lb) end
		end
		focus = f
		if f then
			if f.enter then f:enter(lb) end
		end
	end
	if lb ~= l_button then
		l_button = lb
		if focus then
			if lb then
				if focus.press then focus:press() end
			else
				if focus.release then focus:release() end
			end
		elseif lb then
			begin_painting()
		end
	end
end

-- resize handler

g_width, g_height = 0,0
pinned = {}

function notify_resize(w,h)
	g_width, g_height = w,h
	frames:layout(0,0,w,h) -- lay out root frame.
	for f,v in pairs(pinned) do
		f:layout()
	end
end

-- key handler

keyBind = {}
keyBind[string.byte('Z')] = undo
keyBind[string.byte('Y')] = redo
keyBind[187] = zoomIn   -- '+'
keyBind[189] = zoomOut  -- '-'
keyBind[48] = zoomHome  -- '0'

function notify_key(key, down)
	if down then
        local f = keyBind[key]
        if f then
            f()
        else
            -- print(key)
        end
	end
end

-- timer handler

timers = {}
function notify_timer(id)
    local f = timers[id]
    if f then f() end
end
function setTimer(delay, interval, func)
    local id = StartTimer(delay, interval)
    timers[id] = func
    return id
end
function killTimer(id)
    StopTimer(id)
    timers[id] = nil
end


-- ui library

ids = {}

local __nonce = {0,0,0,0}

function layoutFrame(f,x,y,w,h)
	moveSizeFrame(f,x,y,w,h)
	for i,c in ipairs(f) do c:layout(x,y,w,h) end
end

function newFrame(parent, args)
	local size = args.size or __nonce
	local f = {
		frame = CreateFrame((parent or __nonce).frame),
		id = args.id,
		parent = parent,
		x = size[1], y = size[2], width = size[3], height = size[4],
		layout = args.layout or layoutFrame,
	}
	set_frame_rect(f.frame, f.x, f.y, f.x + f.width, f.y + f.height)
	-- add frame id to global ids
	if args.id then ids[args.id] = f end
	-- spawn child frames
	for i,child in ipairs(args) do
		f[#f+1] = child(f)
	end
	-- apply frame actions
	if args.act then
		for i,func in ipairs(args.act) do func(f,args) end
	end
	return f
end

function showFrame(f)
	if f.layout then
		pinned[f] = true
		f:layout()
	end
	show_frame(f.frame, true)
	f.shown = true
end

function hideFrame(f)
	f.shown = false
	pinned[f] = nil
	show_frame(f.frame, false)
end

function moveFrame(f,x,y)
	f.x, f.y = x,y
	set_frame_rect(f.frame, x, y, x + f.width, y + f.height)
end

function sizeFrame(f,w,h)
	f.width, f.height = w,h
	set_frame_rect(f.frame, f.x, f.y, f.x + w, f.y + h)
end

function moveSizeFrame(f,x,y,w,h)
	f.x, f.y, f.width, f.height = x,y,w,h
	set_frame_rect(f.frame, x, y, x + w, y + h)
end


-- skunkpad logo

function layoutLogo(f,x,y,w,h)
	x=x + math.floor((w or 0) - f.width * 0.8)
	y=y + math.floor((h or 0) - f.height * 0.7)
	moveFrame(f, x, y)
end


-- skunkpad theme

theme = {
	btn_width=32, btn_height=32,
	btn_down=load_resource("btn-down.png"),
	btn_over=load_resource("btn-over.png")
}

theme_sm = {
	btn_width=24, btn_height=24,
	btn_down=load_resource("btn-down-sm.png"),
	btn_over=load_resource("btn-over-sm.png")
}

theme_tiny = {
	btn_width=18, btn_height=18,
	btn_down=load_resource("btn-down-18.png"),
	btn_over=load_resource("btn-over-18.png")
}


-- skunkpad layers

function newLayer()
	local below = findActiveLayer()
	new_layer(below)
	refreshLayers()
	selectLayer(below + 1)
end

function duplicateLayer()
end

function mergeLayer()
end

function deleteLayer()
	local index = findActiveLayer()
	delete_layer(index)
	refreshLayers()
	selectLayer(index - 1)
end

function updateIcon(btn)
	local img
	if btn.value then img=btn.img_down else img=btn.img_up end
	local w,h = btn.width, btn.height
	local iw,ih = get_image_info(img)
	local ix,iy = math.floor(w/2 - iw/2 + .5), math.floor(h/2 - ih/2 + .5)
	set_frame_rect(btn.icon, ix, iy, ix+iw, iy+ih)
	set_frame_image(btn.icon, img)
end

function onToggleRelease(btn)
	btn.value = not btn.value
	updateIcon(btn)
	btn:enter(false) -- redraw
	if btn.func then btn:func() end
end

function toggleShowLayer(btn)
	local layer = btn.parent
	show_layer(layer.index, btn.value)
	-- deselect layer on hide
	if layer.active and not btn.value then
		selectLayer(nil)
	elseif btn.value and findActiveLayer()==0 then
		-- select layer on show
		selectLayer(layer.index)
	end
end

function selectLayer(index)
	local layer
	-- deselect all layers, find layer by index
	for i,f in ipairs(layers) do
		f.active = false
		set_frame_col(f.frame, 1, 1, 1, 0.6)
		if f.index == index then layer = f end
	end
	-- mark active layer
	if layer and layer.showBtn.value then
		layer.active = true
		set_frame_col(layer.frame, 0.5, 0.5, 1, 0.6)
		active_layer(layer.index)
	else
		active_layer(nil)
	end
end

function findActiveLayer()
	for i,f in ipairs(ids.layers) do
		if f.active then return i end
	end
	return 0
end

function onClickLayer(layer)
	selectLayer(layer.index)
end

function makeLayerControl(layers, show, mode, alpha)
	local w,h = layers.width - 4, 48
	local x,y = 2, 2 + layers.num * h
	local f = Panel{size={x,y,w,h-2}}() -- hack
	set_frame_col(f.frame, 1, 1, 1, 1)
	layers.num = layers.num + 1
	f.index = layers.num
	f.hit = true -- hitTest
	f.release = onClickLayer
	--f.showBtn = makeToggle(f, 2, 2, "eye-closed.png", "eye-open.png", theme_sm, toggleShowLayer)
	return f
end

function buildLayerControls()
	local layers = ids.layers
	local i=1
	while true do
		local show,mode,alpha = get_layer_info(i)
		if mode then
			local f = makeLayerControl(layers, show, mode, alpha)
			f.showBtn.value = show
			updateIcon(f.showBtn)
		else
			break
		end
		i=i+1
	end
end

function clearLayerControls()
	local layers = ids.layers
	layers.num = 0
	for i,f in ipairs(layers) do
		-- only destroy layer control panels
		if f.showBtn then destroy_frame(f.frame) end
		layers[i]=nil
	end
end

function refreshLayers()
	clearLayerControls()
	buildLayerControls()
end


-- skunkpad tool bars

function newDocument()
	hideFrame(ids.menu)
	showFrame(ids.tools)
	new_doc(4096, 4096)
	new_layer(0)
	refreshLayers()
	selectLayer(1)
end

function openDocument()
	close_doc()
	refreshLayers()
	showFrame(ids.menu)
	hideFrame(ids.tools)
end

function saveDocument()
	local below = findActiveLayer()
	new_layer(below)
	load_into_layer(below + 1, "test/light_world-1.png")
	new_layer(below + 1)
	load_into_layer(below + 2, "test/test.png")
	refreshLayers()
	selectLayer(below + 2)
end

-- set_brush: mode, sizeMin, sizeMax, spacingPercent, minAlpha, maxAlpha, R,G,B

function selectPencil()
    -- fill one pixel (actually 3x3 circle) 4 times per pixel.
    -- vary transparency to simulate a size less than one.
    -- alpha 0.015 is 3/255, 0.15 is 38/255.
	set_brush("normal", 0.5, 0.5, 0.25, 0.015, 0.15, 0.5, 0.5, 1)
end

function selectBrush()
	-- varies from near-transparent to solid as size increases.
	set_brush("normal", 10, 20, 1, 0.015, 0.5, 0.5, 0.7, 0.2)
end

function selectInk()
	-- 0.25 alpha at 0.25 spacing means 100% after 4 dabs.
	-- intended to give an anti-aliased edge.
	set_brush("normal", 4, 12, 0.25, 0.25, 0.25, 0, 0, 0)
end

function selectEraser()
	-- subtract 100% alpha and colour (hard edge)
	set_brush("subtract", 4, 30, 1, 1, 1, 1, 1, 1)
end


-- layouts

--[[
function clientSized(f)
	f.width = g_width ; f.height = g_height
	set_frame_rect(f.frame, 0, 0, g_width, g_height)
end
]]--

function centerChild(f)
	-- need to lay out the child, then move it into place given its size?
	local child = f[1] -- first child.
	if child then
		local cw,ch = child.width, child.height
		local x,y = math.floor(f.width/2 - cw/2 + .5),
					math.floor(f.height/2 - ch/2 + .5)
		set_frame_rect(child.frame, x, y, x+cw, y+ch)
	end
end

function pinBottom(dist) -- factory.
	return function(f,x,y,w,h)
		y=y + h - dist - f.height
		moveFrame(f,x,y)
	end
end

function layoutHorz(spacing) -- factory.
	return function(f,x,y,w,h)
		local px,py = x + spacing, y + spacing
		local h = 0
		for i,child in ipairs(f) do
			child:layout(px,py,w,h)
			px = px + child.width + spacing
			if child.height > h then h = child.height end
		end
		moveSizeFrame(f,x,y,px,spacing + h + spacing)
	end
end

function layoutVert(spacing) -- factory.
	return function(f)
		local x,y = spacing, spacing
		local w = 0
		for i,child in ipairs(f) do
			child.x = x ; child.y = y
			set_frame_rect(child.frame, x, y,
				x + child.width, y + child.height)
			if child.layout then child:layout() end
			y = y + child.height + spacing
			if child.width > w then w = child.width end
		end
		f.width = spacing + w + spacing ; f.height = y
		set_frame_rect(f.frame, f.x, f.y, f.x + f.width, f.y + f.height)
	end
end


-- actions

function buttonLogic(f,args) -- act.
	f.toggle = args.toggle
	f.theme = args.theme
	f.func = args.func
	f.hit = true -- receive mouse events
	f.down = false
	function f:enter(down)
		if down or (self.toggle and self.down) then
			set_frame_image(self.frame, self.theme.btn_down)
		else
			set_frame_image(self.frame, self.theme.btn_over)
		end
	end
	function f:leave()
		if self.down then
			set_frame_image(self.frame, self.theme.btn_down)
		else
			set_frame_image(self.frame, nil)
		end
	end
	function f:press()
		set_frame_image(self.frame, self.theme.btn_down)
	end
	function f:release()
		if self.toggle then
			self.down = not self.down
		elseif self.radio then
			self.down = true
			self.radio:radioSelected(self)
		end
		self:enter(false) -- redraw
		if self.func then self:func() end
	end
	f:leave() -- initial state.
end

function radioGroup(f, args) -- act.
	for i,b in ipairs(f) do b.radio = f end
	function f:radioSelected(btn)
		for i,b in ipairs(f) do
			if b ~= btn then
				b.down = false
				b:leave(false) -- redraw
			end
		end
	end
end

function tabToggle(f, args) -- act.
	local uiFadeStep = 0.2
	local uiAnimRate = 1/16 -- frame time in seconds
	local uiVisible = true
	local uiAlpha = 1
	local uiTimer = nil

	function animateUI()
		if uiVisible then
			uiAlpha = uiAlpha + uiFadeStep
			if uiAlpha >= 1 then
				uiAlpha = 1
				killTimer(uiTimer)
				uiTimer = nil
			end
		else
			uiAlpha = uiAlpha - uiFadeStep
			if uiAlpha <= 0 then
				uiAlpha = 0
				killTimer(uiTimer)
				uiTimer = nil
			end
		end
		SetFrameAlpha(f.frame, uiAlpha)
	end

	function toggleUI()
		uiVisible = not uiVisible
		if not uiTimer then uiTimer = setTimer(0, uiAnimRate, animateUI) end
	end

	keyBind[9] = toggleUI
end


-- templates.

function Box(args)
	return function(parent)
		local f = newFrame(parent, args)
		if not args.hide then showFrame(f) end
		return f
	end
end

function Panel(args)
	return function(parent)
		local f = newFrame(parent, args)
		set_frame_col(f.frame, 0.8, 0.8, 0.8, 0.8)
		if not args.hide then showFrame(f) end
		return f
	end
end

function Image(args)
	-- load image resource if specified
	local img,w,h
	if args.src then
		img = load_resource(args.src)
		w,h = get_image_info(img)
		-- set frame size unless already specified
		if not args.size then args.size = {0,0,w,h} else
			if not args.size[3] then args.size[3] = w end
			if not args.size[4] then args.size[4] = h end
		end
	end
	return function(parent)
		local f = newFrame(parent, args)
		if img then set_frame_image(f.frame, img) end
		if not args.hide then showFrame(f) end
		return f
	end
end

function Button(args)
	-- add child icon if img is specified.
	if args.img then
		local imgSize
		if args.imgSize then imgSize = {0,0,unpack(args.imgSize,1,2)} end
		args[#args+1] = Image{src=args.img, size=imgSize}
	end
	-- add buttonLogic action
	if not args.act then args.act = {} end
	args.act[#args.act+1] = buttonLogic
	-- derive button size from theme
	if not args.size then args.size = {0,0,args.theme.btn_width,args.theme.btn_height} end
	-- center child by default
	if not args.layout then args.layout = centerChild end
	return Box(args)
end


-- skunkpad

template = Box{id="root",
	Box{id="menu",
		Image{id="logo", src="easelHearts.png", layout=layoutLogo},
	},
	Box{id="tools",
		Box{id="fader", act={tabToggle},
			Panel{id="menubar", size={10, 10, 0, 0}, layout=layoutHorz(2),
				Button{theme=theme_sm, img="new.png", tip="New", func=newDocument},
				Button{theme=theme_sm, img="open.png", tip="Open", func=openDocument},
				Button{theme=theme_sm, img="save.png", tip="Save", func=saveDocument},
			},
			Panel{id="brushes", size={10, 60, 0, 0}, layout=layoutVert(2), act={radioGroup},
				Button{theme=theme, imgSize={24,24}, img="pencil_32.png", tip="Pencil", func=selectPencil},
				Button{theme=theme, imgSize={24,24}, img="paintbrush_32.png", tip="Paintbrush", func=selectBrush},
				Button{theme=theme, imgSize={24,24}, img="pen_32.png", tip="Ink", func=selectInk},
				Button{theme=theme, imgSize={24,24}, img="eraser_32.png", tip="Eraser", func=selectEraser},
			},
			Panel{id="layerWnd", size={10, 300, 200, 2+4*48+20}, layout=pinBottom(10),
				Box{id="layers", size={0, 0, 200, 2+4*48} },
				Box{id="layerTb", size={0, 2+4*48, 0, 0}, layout=layoutHorz(2),
					Button{theme=theme_tiny, img="plus.png", func=newLayer},
					Button{theme=theme_tiny, img="duplicate.png", func=duplicateLayer},
					Button{theme=theme_tiny, img="merge.png", func=mergeLayer},
					Button{theme=theme_tiny, img="cross.png", func=deleteLayer},
				},
			},
		},
	},
}

frames = template()
