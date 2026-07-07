#include "GraphicsEngine.h"

SDL_Renderer * GraphicsEngine::renderer = nullptr;

GraphicsEngine::GraphicsEngine() : fpsAverage(0), fpsPrevious(0), fpsStart(0), fpsEnd(0), drawColor(toSDLColor(0, 0, 0, 255)) {
	if (!SDL_CreateWindowAndRenderer("The X-CUBE 2D Game Engine",
		DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 0, &window, &renderer)) {
		throw EngineException("Failed to create window and renderer", SDL_GetError());
	}
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

	// although not necessary, SDL doc says to prevent hiccups load it before using
	// Actually in SDL3_image IMG_Init might not exist or work differently, wait, let's keep it if it works, otherwise wait IMG_Init still exists. Wait, it failed in the build.
	// Oh, IMG_INIT_PNG does not exist in SDL3_image. We can just call IMG_Init(0) or drop the flag. wait, SDL_image in SDL3 automatically initializes image formats as needed. Let's just remove the IMG_Init call if it's not needed, or replace it.
	// Actually, let's just initialize SDL_ttf and drop IMG_Init.
	if (!TTF_Init())
		throw EngineException("Failed to init SDL_ttf", SDL_GetError());
}

GraphicsEngine::~GraphicsEngine() {
#ifdef __DEBUG
	debug("GraphicsEngine::~GraphicsEngine() started");
#endif

	TTF_Quit();
	SDL_DestroyWindow(window);
	SDL_Quit();

#ifdef __DEBUG
	debug("GraphicsEngine::~GraphicsEngine() finished");
#endif
}

void GraphicsEngine::setWindowTitle(const char * title) {
	SDL_SetWindowTitle(window, title);
#ifdef __DEBUG
	debug("Set window title to:", title);
#endif
}

void GraphicsEngine::setWindowTitle(const std::string & title) {
	SDL_SetWindowTitle(window, title.c_str());
#ifdef __DEBUG
	debug("Set window title to:", title.c_str());
#endif
}

void GraphicsEngine::setWindowIcon(const char *iconFileName) {
	SDL_Surface * icon = IMG_Load(iconFileName);
	if (nullptr == icon) {
		std::cout << "Failed to load icon: " << iconFileName << std::endl;
		std::cout << "Aborting: GraphicsEngine::setWindowIcon()" << std::endl;
		return;
	}
	SDL_SetWindowIcon(window, icon);
#ifdef __DEBUG
	debug("Set Window Icon to", iconFileName);
#endif
	SDL_DestroySurface(icon);
}

void GraphicsEngine::setFullscreen(bool b) {
	SDL_SetWindowFullscreen(window, b);
}

void GraphicsEngine::setVerticalSync(bool b) {
	if (!SDL_SetHint(SDL_HINT_RENDER_VSYNC, b ? "1" : "0")) {
		std::cout << "Failed to set VSYNC" << std::endl;
		std::cout << SDL_GetError() << std::endl;
	}
#ifdef __DEBUG
	debug("Current VSYNC:", SDL_GetHint(SDL_HINT_RENDER_VSYNC));
#endif
}

void GraphicsEngine::setDrawColor(const SDL_Color & color) {
	drawColor = color;
	SDL_SetRenderDrawColor(renderer, drawColor.r, drawColor.g, drawColor.b, 255);	// may need to be adjusted for allowing alpha
}

void GraphicsEngine::setWindowSize(const int &w, const int &h) {
	SDL_SetWindowSize(window, w, h);
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
#ifdef __DEBUG
	debug("Set Window W", w);
	debug("Set Window H", h);
#endif
}

Dimension2i GraphicsEngine::getCurrentWindowSize() {
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	return Dimension2i(w, h);
}

Dimension2i GraphicsEngine::getMaximumWindowSize() {
	const SDL_DisplayMode *current = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
	if (current != nullptr) {
		return Dimension2i(current->w, current->h);
	}
	else {
		std::cout << "Failed to get window data" << std::endl;
		std::cout << "GraphicsEngine::getMaximumWindowSize() -> return (0, 0)" << std::endl;
		return Dimension2i();
	}
}

void GraphicsEngine::showInfoMessageBox(const std::string & info, const std::string & title) {
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title.c_str(), info.c_str(), window);
}

void GraphicsEngine::clearScreen() {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawColor(renderer, drawColor.r, drawColor.g, drawColor.b, 255);	// may need to be adjusted for allowing alpha
}

void GraphicsEngine::showScreen() {
	SDL_RenderPresent(renderer);
}

void GraphicsEngine::useFont(TTF_Font * _font) {
	if (nullptr == _font) {
#ifdef __DEBUG
		debug("GraphicsEngine::useFont()", "font is null");
#endif
		return;
	}

	font = _font;
}

void GraphicsEngine::setFrameStart() {
	fpsStart = SDL_GetTicks();
}

void GraphicsEngine::adjustFPSDelay(const Uint32 &delay) {
	fpsEnd = SDL_GetTicks() - fpsStart;
	if (fpsEnd < delay) {
		SDL_Delay(delay - fpsEnd);
	}

	Uint32 fpsCurrent = 1000 / (SDL_GetTicks() - fpsStart);
	fpsAverage = (fpsCurrent + fpsPrevious + fpsAverage * 8) / 10;	// average, 10 values / 10
	fpsPrevious = fpsCurrent;
}

Uint32 GraphicsEngine::getAverageFPS() {
	return fpsAverage;
}

SDL_Texture * GraphicsEngine::createTextureFromSurface(SDL_Surface * surf) {
	return SDL_CreateTextureFromSurface(renderer, surf);
}

SDL_Texture * GraphicsEngine::createTextureFromString(const std::string & text, TTF_Font * _font, SDL_Color color) {
	SDL_Texture * textTexture = nullptr;
	SDL_Surface * textSurface = TTF_RenderText_Blended(_font, text.c_str(), 0, color);
	if (textSurface != nullptr) {
		textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
		SDL_DestroySurface(textSurface);
	}
	else {
		std::cout << "Failed to create texture from string: " << text << std::endl;
		std::cout << SDL_GetError() << std::endl;
	}

	return textTexture;
}

void GraphicsEngine::setDrawScale(const Vector2f & v) {
	SDL_SetRenderScale(renderer, v.x, v.y);
}

/* ALL DRAW FUNCTIONS */
/* overloads explicitly call SDL funcs for better performance hopefully */

void GraphicsEngine::drawRect(const Rectangle2 & rect) {
	SDL_FRect frect = { (float)rect.x, (float)rect.y, (float)rect.w, (float)rect.h };
	SDL_RenderRect(renderer, &frect);
}

void GraphicsEngine::drawRect(const Rectangle2 & rect, const SDL_Color & color) {
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
	SDL_FRect frect = { (float)rect.x, (float)rect.y, (float)rect.w, (float)rect.h };
	SDL_RenderRect(renderer, &frect);
	SDL_SetRenderDrawColor(renderer, drawColor.r, drawColor.g, drawColor.b, 255);
}

void GraphicsEngine::drawRect(const SDL_Rect * rect, const SDL_Color & color) {
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
	SDL_FRect frect = { (float)rect->x, (float)rect->y, (float)rect->w, (float)rect->h };
	SDL_RenderRect(renderer, &frect);
	SDL_SetRenderDrawColor(renderer, drawColor.r, drawColor.g, drawColor.b, 255);
}

void GraphicsEngine::drawRect(const SDL_Rect * rect) {
	SDL_FRect frect = { (float)rect->x, (float)rect->y, (float)rect->w, (float)rect->h };
	SDL_RenderRect(renderer, &frect);
}

void GraphicsEngine::drawRect(const int &x, const int &y, const int &w, const int &h) {
	SDL_FRect frect = { (float)x, (float)y, (float)w, (float)h };
	SDL_RenderRect(renderer, &frect);
}

void GraphicsEngine::fillRect(const SDL_Rect * rect) {
	SDL_FRect frect = { (float)rect->x, (float)rect->y, (float)rect->w, (float)rect->h };
	SDL_RenderFillRect(renderer, &frect);
}

void GraphicsEngine::fillRect(const int &x, const int &y, const int &w, const int &h) {
	SDL_FRect frect = { (float)x, (float)y, (float)w, (float)h };
	SDL_RenderFillRect(renderer, &frect);
}

void GraphicsEngine::drawPoint(const Point2 & p) {
	SDL_RenderPoint(renderer, p.x, p.y);
}

void GraphicsEngine::drawLine(const Line2i & line) {
	SDL_RenderLine(renderer, line.start.x, line.start.y, line.end.x, line.end.y);
}

void GraphicsEngine::drawLine(const Point2 & p0, const Point2 & p1) {
	SDL_RenderLine(renderer, p0.x, p0.y, p1.x, p1.y);
}

void GraphicsEngine::drawCircle(const Point2 & center, const float & radius) {
	for (float i = 0.0f; i < 2*3.14159265358979323846; i += PI_OVER_180) {
		int x = (int)(center.x + radius * SDL_cosf(i));
		int y = (int)(center.y + radius * SDL_sinf(i));
		SDL_RenderPoint(renderer, x, y);
	}
}

void GraphicsEngine::drawEllipse(const Point2 & center, const float & radiusX, const float & radiusY) {
	for (float i = 0.0f; i < 2 * 3.14159265358979323846; i += PI_OVER_180) {
		int x = (int)(center.x + radiusX * SDL_cosf(i));
		int y = (int)(center.y + radiusY * SDL_sinf(i));
		SDL_RenderPoint(renderer, x, y);
	}
}

void GraphicsEngine::drawTexture(SDL_Texture * texture, const SDL_Rect * src, const SDL_Rect * dst, const double & angle, const SDL_FPoint * center, SDL_FlipMode flip) {
	SDL_FRect fsrc, fdst;
	SDL_FRect *pfsrc = nullptr, *pfdst = nullptr;
	if (src) {
		fsrc = { (float)src->x, (float)src->y, (float)src->w, (float)src->h };
		pfsrc = &fsrc;
	}
	if (dst) {
		fdst = { (float)dst->x, (float)dst->y, (float)dst->w, (float)dst->h };
		pfdst = &fdst;
	}
	SDL_RenderTextureRotated(renderer, texture, pfsrc, pfdst, angle, center, flip);
}

void GraphicsEngine::drawTexture(SDL_Texture * texture, const SDL_Rect * dst, SDL_FlipMode flip) {
	SDL_FRect fdst;
	SDL_FRect *pfdst = nullptr;
	if (dst) {
		fdst = { (float)dst->x, (float)dst->y, (float)dst->w, (float)dst->h };
		pfdst = &fdst;
	}
	SDL_RenderTextureRotated(renderer, texture, nullptr, pfdst, 0.0, nullptr, flip);
}

void GraphicsEngine::drawText(const std::string & text, const int &x, const int &y) {
	SDL_Texture * textTexture = createTextureFromString(text, font, drawColor);
	float w, h;
	SDL_GetTextureSize(textTexture, &w, &h);
	SDL_Rect dst = { x, y, (int)w, (int)h };
	drawTexture(textTexture, &dst);
	SDL_DestroyTexture(textTexture);
}