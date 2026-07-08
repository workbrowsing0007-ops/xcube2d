#ifndef __MY_ENGINE_H__
#define __MY_ENGINE_H__
#include "../ResourceManager.h"																									// For resource loading
#include <unordered_map>																										// For component storage
#include <utility>																												// for std::pair
#include <unordered_set>																										// for unordered set
#include <algorithm>																											// for std::stable_sort

static constexpr int DEFAULT_ENTITY_ID = { -1 };																				// Default entity ID
static constexpr float DEFAULT_ENTITY_SCALE = { 1.0f }, DEFAULT_UNIT_SPEED = { 100 }, DEFAULT_PC_SPEED = { 200 },				// Default scales and speeds
deltaTime = { 1.0f / 60.0f }, OFFSCREEN_X = { -1000.0f }, OFFSCREEN_Y = { -1000.0f }, DEFAULT_PROJECTILE_SPEED = { 800.0f };	// delta time and offscreen positions
static constexpr Uint32 DEFAULT_MAX_HEALTH = { 100 }, DEFAULT_AMMO = { 10 }, DEFAULT_MAX_AMMO = { 50 }, TILE_SIZE = { 16 },		// Default health, ammo and tile size
STAT_CHANGE_COOLDOWN = { 250 }, BAR_HEIGHT = { 4 }, DEFAULT_UNIT_DAMAGE = { 10 }, DEFAULT_NPC_CHASE_RANGE = { 256 * 256 },		// stat change cooldown, health bar height, damage and NPC chase range
DEFAULT_NPC_SCORE_VALUE = { 10 }, DEFAULT_SFX_VOLUME = { 10 }, DEFAULT_FONT_SIZE = { 24 }, CAMERA_SMOOTHING_FACTOR = { 6 },		// default score value, sfx volume, font size and camera smoothing
BACKGROUND_LAYER = { 0 }, GROUND_LAYER = { 1 }, OBJECT_LAYER = { 2 };															// default rendering layers
static constexpr size_t DEFAULT_PROJECTILES_PER_OWNER = { 50 };																	// default projectile pool size per owner

class MyEngineSystem {
	friend class XCube2Engine;																									// Friend class declaration
private:
	MyEngineSystem();																											// Constructor
	using Entity = std::uint32_t;																								// Entity type
	enum class EntityTag { Unknown = 0, PC, NPC, AMMO, HEALTH, PROJECTILE, ENDLEVEL };											// Entity tags
	template<typename T>																										// Template for component map
	using ComponentMap = std::unordered_map<Entity, T>;																			// Component storage map
	struct PCTag {};																											// PC Tag
	struct NPCTag {};																											// NPC Character Tag
	struct AmmoPickupTag {};																									// Ammo Pickup Tag
	struct HealthPickupTag {};																									// Health Pickup Tag
	struct ProjectileTag { Entity owner = { 0 }; };																				// Projectile Tag with owner entity
	struct EndLevelTag {};																										// End Level Tag
	struct Transform {																											// A structure to hold transform data
		Vector2f startPosition = {}, position = {}, newPosition = {};															// Positions					
		float scale = DEFAULT_ENTITY_SCALE;																						// Scale
		int rotation = {}, layer = {};																							// Rotation and rendering layer
		bool initialFlipH = false, flipH = false, active = true;																// Flipping and active state
	};
	struct Velocity { float x = {}, y = {}; };																					// Velocity structure
	struct Sprite {																												// Sprite structure
		SDL_Texture* texture = nullptr;																							// Texture pointer
		int frameW = {}, frameH = {}, frameCount = 1, textureWidth = {}, textureHeight = {}, startFrame = {};					// Frame dimensions and count
		bool loop = true;																										// Looping flag
		float scale = DEFAULT_ENTITY_SCALE;																						// Scale
	};
	struct Animation {																											// Animation structure
		std::string name;																										// Animation name
		bool loop = true;																										// Looping flag
		int frameCount = 1, currentFrame = {};																					// Frame count and current frame
		float frameDuration = 0.1f, animTimer = {};																				// Frame duration and timer
	};
	struct AnimationState {																										// Animation state structure
		std::string idle_up = {}, idle_down = {}, idle_right = {}, walk_up = {}, walk_down = {}, walk_right = {},				// idle and walk animations
			death_up = {}, death_down = {}, death_right = {}, previousAnimation = {};											// death and previous animation
	};
	struct Audio { std::string damageSound = {}, attackingSound = {}; };														// Audio component structure with sound names
	struct Health {																												// Health structure
		int currentHealth = DEFAULT_MAX_HEALTH, maxHealth = DEFAULT_MAX_HEALTH;													// Health values
		Uint32 lastHealthChangeTime = STAT_CHANGE_COOLDOWN;																		// Last health change time
	};
	struct HealthBar { SDL_Rect backgroundRect = {}, healthRect = {}; };														// Health bar structure
	struct Collider { SDL_Rect rect = {}; };																					// Collider structure
	struct Damage { int amount = DEFAULT_UNIT_DAMAGE; Uint32 lastDamageDealtTime = STAT_CHANGE_COOLDOWN; };						// Damage structure
	struct Speed { float value = DEFAULT_UNIT_SPEED; };																			// Speed structure
	struct Ammo { int currentAmmo = DEFAULT_AMMO, maxAmmo = DEFAULT_MAX_AMMO; Uint32 lastFireTime = STAT_CHANGE_COOLDOWN; };	// Ammo structure
	struct Input { float x = {}, y = {}; };																						// Input structure
	struct ScoreValue { int amount = {}; };																						// Score value structure
	struct Component																											// Component storage struct
	{
		ComponentMap<Transform> transforms;																						// Transform Component storage
		ComponentMap<Velocity> velocities;																						// Velocity Component storage
		ComponentMap<Sprite> sprites;																							// Sprite Component storage
		ComponentMap<Animation> animations;																						// Animation Component storage
		ComponentMap<PCTag> players;																							// Player Tag Component storage
		ComponentMap<NPCTag> npcs;																								// NPC Tag Component storage
		ComponentMap<AmmoPickupTag> ammoPickups;																				// Ammo Pickup Tag Component storage
		ComponentMap<HealthPickupTag> healthPickups;																			// Health Pickup Tag
		ComponentMap<ProjectileTag> projectiles;																				// Projectile Component storage
		ComponentMap<EndLevelTag> endLevels;																					// End Level Tag Component storage
		ComponentMap<Health> healths;																							// Health Component storage
		ComponentMap<Collider> colliders;																						// Collider Component storage
		ComponentMap<Damage> damages;																							// Damage Component storage
		ComponentMap<Speed> speeds;																								// Speed Component storage
		ComponentMap<Ammo> ammos;																								// Ammo Component storage
		ComponentMap<HealthBar> healthBars;																						// HealthBar Component storage
		ComponentMap<Input> inputs;																								// Input Component storage
		ComponentMap<AnimationState> animationStates;																			// AnimationState Component storage
		ComponentMap<bool>dying;																								// Dying state Component storage
		ComponentMap<Audio> audios;																								// Audio Component storage
		ComponentMap<ScoreValue> scores;																						// Score Component storage
	};
	Component component;
	std::map<std::string, Sprite> loadedSprites;																				// Loaded sprite data keyed by name
	std::map<std::string, Mix_Chunk*> loadedSounds;																				// Loaded sounds
	std::unordered_map<Entity, std::vector<Entity>> projectilePools;															// owner pool of projectile entity IDs
	struct Tile { int x = {}, y = {}; std::string spriteName; };																// Tile structure with position and sprite name
	std::vector<Tile> groundTiles;																								// A list of ground tiles
	std::unordered_set<Entity> activeEntities;																					// currently active entities
	std::unordered_set<Entity> entitiesToDestroy;																				// entities queued for destruction
	Uint32 now = {};																											// Current time in milliseconds
	Uint32 score = {};																											// Global score
	Vector2f cameraPosition = {};																								// camera world position 
	float cameraSmoothing = CAMERA_SMOOTHING_FACTOR;																			// camera smoothing factor
	Uint32 currentLevel = {}, levelsCount = {};																					// current level index and total levels
	bool levelChanging = {}, gameCompleted = {};																				// level changing and game completed flags
	Uint32 worldWidth = {}, worldHeight = {};																					// world width and height in pixels 
	// PRIVATE METHODS
	void flushDestroyedEntities();																								// actually remove enqueued entities
	void destroyEntity(Entity entity) { entitiesToDestroy.insert(entity); }														// Destroy entity
	void movementSystem(Component& com, float deltaTime = deltaTime);															// Movement system
	void animationSystem(Component& com, float deltaTime = deltaTime);															// Animation system
	void updateAnimationStates(Component& com, float deltaTime = deltaTime);													// Update animation states
	void renderHealthBar(std::shared_ptr<GraphicsEngine> gfx, Entity entity, int posX, int posY, int width, int height);		// Render health bar
	void renderTiles(std::shared_ptr<GraphicsEngine>);																			// Render ground tiles
	void collisionSystem(Component& com, float deltaTime = deltaTime);															// Collision system
	void aiSystem(Component& com, Entity playerEntity, float deltaTime = deltaTime);											// AI system
	void changeEntityHealth(Entity entity, int amount);																			// Change entity health
	void processCollisionEntities(Component& com, Entity primary, Entity other, Uint32 now);									// Process collision between two entities
	void playAudio(const std::string& name, int volume = -1, int loops = 0, int channel = -1);									// Play audio
	void handleDeath(Entity entity, Health& health);																			// Respawn entity
	void deactivateProjectile(Entity proj);																						// deactivate projectile
	void updateCamera(std::shared_ptr<GraphicsEngine> gfx, float deltaTime = deltaTime);										// update camera position
	void increaseAmmo(Entity attacker, Entity victim);																			// increase ammo for owner
	void processPendingDeaths();																								// Check dying entities and finalize when anim done
	void finaliseDeath(Entity entity);																							// Perform the actual death completion work
	bool isProjectileOwner(Entity entity, Entity other);																		// check if projectile owner
	void setEntityColliderRect(Entity entity, float posX, float posY, int width, int height);									// set entity collider rectangle
	template<typename T>																										// Template for getting valid component
	bool isValidComponent(Entity entity, ComponentMap<T>& comp);																// check if entity has valid component
public:
	~MyEngineSystem();																											// Destructor
	Entity createEntity() { static Entity next{}; Entity id = next++; activeEntities.insert(id); return id; };					// Create a new entity ID
	void loadSprite(const std::string& name, const std::string& filename, int frameW, int frameH, int frames, int startFrame = 0, bool loop = false, float scale = 1, SDL_Color transparent = { 255,255,255,255 });	// Load sprite from file
	void loadSound(const std::string& name, const std::string& filename);														// Load sound
	void render(std::shared_ptr<GraphicsEngine> gfx);																			// Render all entities
	void update(float deltaTime = deltaTime, int playerEntityId = 1);															// Update all systems
	void addGroundTile(const std::string& spriteName, int x, int y);															// Add ground tile
	void fireProjectile(Entity owner, const Vector2f& startPos, const Vector2f& targetPos);										// fire a projectile from owner
	void setEntityInput(Entity entity, float x, float y);																		// Set entity input
	void initProjectilePool(Entity owner, size_t poolSize = DEFAULT_PROJECTILES_PER_OWNER);										// initialise projectile pool for owner
	void attachSprite(Entity entity, const std::string& spriteName);															// Attach sprite to entity
	int roundToInt(float value) { return static_cast<int>(std::round(value)); }													// round helper
	void clearLevelExcept(Entity keep);
	// ADD COMPONENTS
	void addComponentPCTag(Entity entity) { component.players[entity] = PCTag(); }												// Set PC tag
	void addComponentNPCTag(Entity entity) { component.npcs[entity] = NPCTag(); }												// Set NPC tag
	void addComponentAmmoPickupTag(Entity entity) { component.ammoPickups[entity] = AmmoPickupTag(); }							// Set ammo pickup tag
	void addComponentHealthPickupTag(Entity entity) { component.healthPickups[entity] = HealthPickupTag(); }					// Set health pickup tag
	void addComponentEndLevelTag(Entity entity) { component.endLevels[entity] = EndLevelTag(); }								// Set end level tag	
	void addComponentTransform(Entity entity, const Vector2f& position, float scale = DEFAULT_ENTITY_SCALE, int rotation = 0, int layer = 0, bool initialFlipH = false) { component.transforms[entity] = Transform{ position, position, position, scale, rotation, layer, initialFlipH, false, true }; }
	void addComponentVelocity(Entity entity, float x = {}, float y = {}) { component.velocities[entity] = Velocity{ x, y }; }	// Set velocity	
	void addComponentSpeed(Entity entity, float speed = DEFAULT_UNIT_SPEED) { component.speeds[entity] = Speed{ speed }; }		// Set speed
	void addComponentCollider(Entity entity, float x, float y, int width = TILE_SIZE, int height = TILE_SIZE) { component.colliders[entity] = Collider{ SDL_Rect{ roundToInt(x), roundToInt(y), width, height } }; }	// Set collider
	void addComponentHealth(Entity entity, int currentHealth = DEFAULT_MAX_HEALTH, int maxHealth = DEFAULT_MAX_HEALTH) { component.healths[entity] = Health{ currentHealth, maxHealth }; }	// Set health
	void addComponentHealthBar(Entity entity) { component.healthBars[entity] = HealthBar(); }									// Set health bar
	void addComponentAmmo(Entity entity, int currentAmmo, int maxAmmo = DEFAULT_MAX_AMMO) { component.ammos[entity] = Ammo{ currentAmmo, maxAmmo }; }
	void addComponentDamage(Entity entity, int amount = DEFAULT_UNIT_DAMAGE) { component.damages[entity] = Damage{ amount }; }	// Set damage
	void addComponentInput(Entity entity) { component.inputs[entity] = Input{}; }												// Set input
	void addComponentDying(Entity entity) { component.dying[entity] = false; }													// Set dying state
	void addComponentIdleAnimations(Entity entity, std::string = "", std::string = "", std::string = "");						// Set idle animations
	void addComponentWalkAnimations(Entity entity, std::string = "", std::string = "", std::string = "");						// Set walk animations
	void addComponentDeathAnimations(Entity entity, std::string = "", std::string = "", std::string = "");						// Set death animations
	void addComponentAudio(Entity entity, std::string damageSound = "", std::string attackingSound = "");						// Set audio component
	void addComponentScoreValue(Entity entity, int amount) { component.scores[entity] = ScoreValue{ amount }; }					// set score value component
	void addComponentProjectileTag(Entity entity, Entity owner) { component.projectiles[entity] = ProjectileTag{ owner }; }		// Set projectile tag
	// GETTERS
	Uint32 getCurrentLevel() const { return currentLevel; }																		// get current level index
	bool isLevelChanging() const { return levelChanging; }																		// is level changing
	bool isGameCompleted() const { return gameCompleted; }																		// is game completed
	int getNPCCount() const { return static_cast<int>(component.npcs.size()); };												// get current NPC count
	int getScore() const { return score; };																						// Get current score
	EntityTag getEntityTag(Entity entity);																						// Get entity tag
	Vector2f MyEngineSystem::getCameraPosition() const { return cameraPosition; };												// get camera position
	int getAmmo(Entity entity) { return (isValidComponent(entity, component.ammos)) ? component.ammos[entity].currentAmmo : -1; }	// Get entity ammo
	int getEntityHealth(Entity entity) { return (isValidComponent(entity, component.healths)) ? component.healths[entity].currentHealth : -1; }	// Get entity health
	Vector2f getEntityPosition(Entity entity) { return (isValidComponent(entity, component.transforms)) ? component.transforms[entity].position : Vector2f{}; }	// Get entity position
	SDL_Rect getEntityColliderRect(Entity entity) { return (isValidComponent(entity, component.colliders)) ? component.colliders[entity].rect : SDL_Rect{}; }	// Get entity collider rectangle
	std::vector<Entity> getAllEndLevelTriggers() { std::vector<Entity> triggers; for (const auto& pair : component.endLevels)  triggers.push_back(pair.first); return triggers; } // get all end level triggers
	// SETTERS	
	void setEntityPosition(Entity entity, const Vector2f& position) { if (isValidComponent(entity, component.transforms)) { component.transforms[entity].position = position; component.transforms[entity].newPosition = position; component.transforms[entity].startPosition = position; } } // Set entity position
	void setWorldDimensions(Uint32 width, Uint32 height) { worldWidth = width; worldHeight = height; }							// set world dimensions
	void setLevelsCount(Uint32 count) { levelsCount = count; }																	// set total number of levels
	void setLevelChanging(bool value) { levelChanging = value; }																// set level changing flag
};

#endif 