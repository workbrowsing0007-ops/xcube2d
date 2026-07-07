#include "XCube2d.h"

std::shared_ptr<XCube2Engine> XCube2Engine::instance = nullptr;

XCube2Engine::XCube2Engine() {
	std::cout << "Initializing X-CUBE 2D v" << _ENGINE_VERSION_MAJOR << "." << _ENGINE_VERSION_MINOR << std::endl;

#ifdef __DEBUG
	#if defined(_WIN32)
		debug("WIN32");
	#elif defined(__linux__)
		debug("LINUX");
	#elif defined(__APPLE__)
		debug("MACOSX");
	#endif
#endif

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
		throw EngineException("SDL_Init()", SDL_GetError());

#ifdef __DEBUG
	debug("SDL_Init() successful");
#endif

	std::cout << "Logical CPU cores: " << SDL_GetNumLogicalCPUCores() << std::endl;
	std::cout << "System RAM: " << SDL_GetSystemRAM() << " MB" << std::endl;
	std::cout << "Running on: " << SDL_GetPlatform() << std::endl;

	int compiled = SDL_VERSION;
	int linked = SDL_GetVersion();

	printf("Compiled against SDL %d.%d.%d\n",
		SDL_VERSIONNUM_MAJOR(compiled), SDL_VERSIONNUM_MINOR(compiled), SDL_VERSIONNUM_MICRO(compiled));
	printf("Linked against SDL %d.%d.%d\n",
		SDL_VERSIONNUM_MAJOR(linked), SDL_VERSIONNUM_MINOR(linked), SDL_VERSIONNUM_MICRO(linked));

	Uint32 ticks = SDL_GetTicks();
	srand(ticks);	// init random seed

#ifdef __DEBUG
	debug("Inited srand() with", ticks);
#endif

	// init subsystems

	gfxInstance = std::shared_ptr<GraphicsEngine>(new GraphicsEngine());

#ifdef __DEBUG
	debug("GraphicsEngine() successful");
#endif

	audioInstance = std::shared_ptr<AudioEngine>(new AudioEngine());

#ifdef __DEBUG
	debug("AudioEngine() successful");
#endif

	eventInstance = std::shared_ptr<EventEngine>(new EventEngine());

#ifdef __DEBUG
	debug("EventEngine() successful");
#endif

	physicsInstance = std::shared_ptr<PhysicsEngine>(new PhysicsEngine());

    myEngineSystemInstance = std::shared_ptr<MyEngineSystem>(new MyEngineSystem());

#ifdef __DEBUG
    debug("MyEngineSystem() successful");
#endif
}

XCube2Engine::~XCube2Engine() {
#ifdef __DEBUG
	debug("XCube2Engine::~XCube2Engine() started");
#endif

	ResourceManager::freeResources();

	eventInstance.reset();
	gfxInstance.reset();

#ifdef __DEBUG
	debug("XCube2Engine::~XCube2Engine() finished");
#endif
}

void XCube2Engine::quit() {
	if (instance)
		instance.reset();
}

std::shared_ptr<XCube2Engine> XCube2Engine::getInstance() {
	if (!instance)
		instance = std::shared_ptr<XCube2Engine>(new XCube2Engine());
	return instance;
}