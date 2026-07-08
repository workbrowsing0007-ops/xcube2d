#include "MyEngineSystem.h"																					// Include header

MyEngineSystem::MyEngineSystem() {																			// Constructor
#ifdef __DEBUG																								// Debug info
	debug("MyEngineSystem constructed");																	// Log construction
#endif																										// Debug info
}

MyEngineSystem::~MyEngineSystem() {																			// Destructor
#ifdef __DEBUG																								// Debug info
	debug("MyEngineSystem::~MyEngineSystem() freeing resources");											// Log destruction
#endif																										// Debug info
	loadedSprites.clear();																					// Clear sprites
	loadedSounds.clear();																					// Clear sounds
	groundTiles.clear();																					// Clear ground tiles
	projectilePools.clear();																				// Clear projectile pools
	entitiesToDestroy.clear(); 																				// Clear entities to destroy
	activeEntities.clear();																					// Clear active entities
}

void MyEngineSystem::update(float deltaTime, int playerEntityId)
{
	now = SDL_GetTicks();																					// get current time
	aiSystem(component, playerEntityId, deltaTime);															// update AI
	movementSystem(component, deltaTime);																	// update movement
	collisionSystem(component, deltaTime);																	// update collisions
	updateAnimationStates(component, deltaTime);															// update animation states
	animationSystem(component, deltaTime);																	// update animations
	processPendingDeaths();																					// handle deaths whose animation finished
	flushDestroyedEntities();																				// flush destroyed entities
}

void MyEngineSystem::aiSystem(Component& com, Entity playerEntity, float deltaTime)
{
	if (playerEntity == 0) return;																			// IF NO PLAYER ENTITY, return
	if (!isValidComponent(playerEntity, com.transforms)) return;											// IF NO PLAYER TRANSFORM, return
	const Transform& pcTransform = com.transforms[playerEntity];											// get player transform
	for (auto& npc : com.npcs) {																			// FOR EACH NPC
		Entity entity = npc.first;																			// get entity
		if (!isValidComponent(entity, com.transforms)) continue;											// IF NO NPC TRANSFORM, skip
		Transform& npcTransform = com.transforms[entity];													// get transform
		float dx = pcTransform.position.x - npcTransform.position.x;										// delta x
		float dy = pcTransform.position.y - npcTransform.position.y;										// delta y
		float distance = dx * dx + dy * dy;																	// distance squared
		if (distance <= DEFAULT_NPC_CHASE_RANGE && distance > 0.0f) {										// IF WITHIN CHASE RANGE AND DISTANCE > 0 to avoid division by zero
			float distSqrt = std::sqrt(distance);															// distance
			float newVelX = dx / distSqrt;																	// normalised x
			float newVelY = dy / distSqrt;																	// normalised y
			com.inputs[entity] = Input{ newVelX, newVelY };													// set input to move towards player
		}
		else com.inputs[entity] = Input{};																	// ELSE stop movement
	}
}

void MyEngineSystem::movementSystem(Component& com, float deltaTime)
{
	for (auto& inputComp : com.inputs) {																	// FOR EACH INPUT
		Entity entity = inputComp.first;																	// get entity
		const Input& input = inputComp.second;																// get input
		float dx = input.x;																					// get input x
		float dy = input.y;																					// get input y
		float length = std::sqrt(dx * dx + dy * dy);														// calculate length
		if (length > 1.0f) { dx /= length; dy /= length; }													// normalise if length > 1
		float speed = DEFAULT_UNIT_SPEED;																	// default speed
		if (isValidComponent(entity, com.speeds)) speed = com.speeds[entity].value;							// IF HAS SPEED COMPONENT, get speed
		com.velocities[entity] = Velocity{ dx * speed, dy * speed };										// set velocity
	}
	for (auto& velocityComp : com.velocities) {																// FOR EACH VELOCITY
		Entity entity = velocityComp.first;																	// get entity
		if (!isValidComponent(entity, com.transforms)) continue;											// IF NO TRANSFORM, skip
		Transform& transform = com.transforms[entity];														// get transform
		if (!transform.active) continue;																	// IF NOT ACTIVE, skip
		const Velocity& velocity = velocityComp.second;														// get velocity
		float attemptedX = transform.position.x + velocity.x * deltaTime;									// calculate attempted X
		float attemptedY = transform.position.y + velocity.y * deltaTime;									// calculate attempted Y
		transform.newPosition = Vector2f(attemptedX, attemptedY);											// set new position
	}
}

void MyEngineSystem::collisionSystem(Component& com, float deltaTime)
{
	for (auto& velocityComp : com.velocities) {																					// FOR EACH VELOCITY
		Entity entity = velocityComp.first;																						// get entity
		Velocity velocity = velocityComp.second;																				// get velocity
		if (!isValidComponent(entity, com.transforms))continue;																	// IF NO TRANSFORM, skip
		if (!isValidComponent(entity, com.colliders)) continue;																	// IF NO COLLIDER, skip
		Transform& transform = com.transforms[entity];																			// get transform
		if (!transform.active) continue;																						// IF NOT ACTIVE, skip
		Collider& collider = com.colliders[entity];																				// get collider
		std::vector<std::pair<Entity, SDL_Rect>> obstacles;																		// vector of obstacles
		obstacles.reserve(com.colliders.size());																				// reserve space
		for (auto& colliderComp : com.colliders) {																				// FOR EACH OTHER COLLIDER
			Entity other = colliderComp.first;																					// get other entity
			if (other == entity) continue;																						// IF SAME ENTITY, skip
			if (isProjectileOwner(entity, other)) continue;																		// IF PROJECTILE OWNER, skip
			if (!isValidComponent(other, com.transforms)) continue;																// IF NO TRANSFORM, skip
			const Transform& otherTransform = com.transforms[other];															// get other transform
			if (!otherTransform.active) continue;																				// IF NOT ACTIVE, skip
			obstacles.emplace_back(other, colliderComp.second.rect);															// add to obstacles
		}
		SDL_Rect rectX = collider.rect;																							// copy collider rect
		rectX.x = roundToInt(transform.newPosition.x);																			// update x position
		for (const auto& obstacle : obstacles) {																				// FOR EACH OBSTACLE
			const Entity other = obstacle.first;																				// get other entity
			const SDL_Rect& obstacleRect = obstacle.second;																		// get obstacle rect
			if (SDL_HasIntersection(&rectX, &obstacleRect) == SDL_TRUE) {														// IF INTERSECTING
				processCollisionEntities(com, entity, other, now);																// process collision
				if (getEntityTag(entity) == EntityTag::PROJECTILE || getEntityTag(other) == EntityTag::PROJECTILE) continue;	// IF PROJECTILE, skip position adjustment
				if (getEntityTag(entity) == EntityTag::ENDLEVEL || getEntityTag(other) == EntityTag::ENDLEVEL) continue;		// IF END LEVEL, skip position adjustment
				if (velocity.x > 0.0f) transform.newPosition.x = float(obstacleRect.x - rectX.w);								// IF MOVING RIGHT, adjust position
				else transform.newPosition.x = float(obstacleRect.x + obstacleRect.w);											// ELSE ADJUST LEFT
				velocity.x = 0.0f;																								// stop horizontal movement
				collider.rect.x = roundToInt(transform.newPosition.x);															// update collider position
				break;																											// exit loop
			}
		}
		transform.position.x = transform.newPosition.x;																			// update position x
		SDL_Rect rectY = collider.rect;																							// copy collider rect
		rectY.y = roundToInt(transform.newPosition.y);																			// update y position
		for (const auto& obstacle : obstacles) {																				// FOR EACH OBSTACLE
			const Entity other = obstacle.first;																				// get other entity
			const SDL_Rect& obstacleRect = obstacle.second;																		// get obstacle rect
			if (SDL_HasIntersection(&rectY, &obstacleRect) == SDL_TRUE) {														// IF INTERSECTING
				processCollisionEntities(com, entity, other, now);																// process collision
				if (getEntityTag(entity) == EntityTag::PROJECTILE || getEntityTag(other) == EntityTag::PROJECTILE) continue;	// IF PROJECTILE, skip position adjustment
				if (getEntityTag(entity) == EntityTag::ENDLEVEL || getEntityTag(other) == EntityTag::ENDLEVEL) continue;		// IF END LEVEL, skip position adjustment
				if (velocity.y > 0.0f) transform.newPosition.y = float(obstacleRect.y - rectY.h);								// IF MOVING DOWN, adjust position
				else transform.newPosition.y = float(obstacleRect.y + obstacleRect.h);											// ELSE ADJUST UP
				velocity.y = 0.0f;																								// stop vertical movement
				collider.rect.y = roundToInt(transform.newPosition.y);															// update collider position
				break;																											// exit loop
			}
		}
		transform.position.y = transform.newPosition.y;																			// update position y
		collider.rect.x = roundToInt(transform.position.x);																		// update collider position x
		collider.rect.y = roundToInt(transform.position.y);																		// update collider position y
		com.velocities[entity] = velocity;																						// update velocity to reflect any changes
	}
}

void MyEngineSystem::processCollisionEntities(Component& com, Entity primary, Entity other, Uint32 now)
{
	if ((getEntityTag(primary) == EntityTag::ENDLEVEL && getEntityTag(other) == EntityTag::PC)											// IF PRIMARY IS ENDLEVEL AND OTHER IS PC
		|| (getEntityTag(other) == EntityTag::ENDLEVEL && getEntityTag(primary) == EntityTag::PC)) {									// OR VICE VERSA
		Entity player = (getEntityTag(primary) == EntityTag::PC) ? primary : other;														// get player entity
		Entity endTrigger = (getEntityTag(primary) == EntityTag::ENDLEVEL) ? primary : other;											// get end level trigger entity
		if (currentLevel + 1 >= levelsCount) {																							// IF LAST LEVEL 
			if (getNPCCount() <= 0) {																									// IF NO NPCS LEFT
				gameCompleted = true;																									// set game completed
				return;																													// return
			}
			else return;																												// return
		}
		++currentLevel;																													// increment level									
		levelChanging = true;																											// set level changing
		if (isValidComponent(endTrigger, com.audios) && !com.audios[endTrigger].attackingSound.empty())									// IF END TRIGGER HAS AUDIO COMPONENT AND ATTACKING SOUND
			playAudio(com.audios[endTrigger].attackingSound, DEFAULT_SFX_VOLUME);														// play sound
		clearLevelExcept(player);																										// clear level except player
		return;																															// return
	}
	Entity attacker = 0, victim = 0;																									// attacker and victim
	if (getEntityTag(primary) == EntityTag::AMMO) { increaseAmmo(primary, other); return; }												// IF PRIMARY IS AMMO, increase ammo and return
	else if (getEntityTag(other) == EntityTag::AMMO) { increaseAmmo(other, primary); return; }											// IF OTHER IS AMMO, increase ammo and return
	else if (getEntityTag(primary) == EntityTag::HEALTH && getEntityTag(other) == EntityTag::NPC										// IF ATTACKER IS HEALTH AND VICTIM IS NPC
		|| getEntityTag(primary) == EntityTag::NPC && getEntityTag(other) == EntityTag::HEALTH) return;									// OR VICE VERSA, return
	else if (getEntityTag(primary) == EntityTag::HEALTH) {																				// IF ATTACKER IS HEALTH PICKUP
		Health& health = com.healths[other];																							// get health of victim	
		if (health.currentHealth >= health.maxHealth) return;																			// IF HEALTH IS MAX, return
		attacker = primary; victim = other;																								// set attacker and victim
	}
	else if (getEntityTag(other) == EntityTag::HEALTH) {																				// IF VICTIM IS HEALTH PICKUP
		Health& health = com.healths[primary];																							// get health of attacker
		if (health.currentHealth >= health.maxHealth) return;																			// IF HEALTH IS MAX, return
		attacker = other; victim = primary;																								// set attacker and victim
	}
	else if (getEntityTag(primary) == EntityTag::PROJECTILE) { attacker = primary; victim = other; }									// IF PRIMARY IS PROJECTILE, set attacker and victim
	else if (getEntityTag(other) == EntityTag::PROJECTILE) { attacker = other; victim = primary; }										// IF OTHER IS PROJECTILE, set attacker and victim
	else if (getEntityTag(primary) == EntityTag::NPC && getEntityTag(other) == EntityTag::PC) { attacker = primary; victim = other; }	// IF PRIMARY IS NPC AND OTHER IS PC, set attacker and victim
	else if (getEntityTag(other) == EntityTag::NPC && getEntityTag(primary) == EntityTag::PC) { attacker = other; victim = primary; }	// IF OTHER IS NPC AND PRIMARY IS PC, set attacker and victim
	if (attacker <= 0 && victim < 0) return;																							// IF NO ATTACKER OR VICTIM, return
	std::string audioString = {};																										// audio string
	if (!isValidComponent(attacker, com.damages)) return;																				// IF ATTACKER HAS NO DAMAGE COMPONENT, return
	Damage& damage = com.damages[attacker];																								// get damage component
	if (now - damage.lastDamageDealtTime < STAT_CHANGE_COOLDOWN) return;																// IF WITHIN COOLDOWN, return
	if (damage.amount > 0) {													 														// IF DAMAGE AMOUNT > 0
		if (isValidComponent(victim, com.audios)) {																						// IF VICTIM HAS AUDIO COMPONENT								
			audioString = com.audios[victim].damageSound;																				// get damage sound
			if (!audioString.empty() && loadedSounds.count(audioString)) playAudio(audioString, DEFAULT_SFX_VOLUME);					// play sound
		}
	}
	if (isValidComponent(attacker, com.audios)) {																						// IF ATTACKER HAS AUDIO COMPONENT
		audioString = com.audios[attacker].attackingSound;																				// get attacking sound
		if (!audioString.empty() && loadedSounds.count(audioString)) playAudio(audioString, DEFAULT_SFX_VOLUME);						// play sound
	}
	changeEntityHealth(victim, -damage.amount);																							// reduce victim health
	if (getEntityTag(attacker) == EntityTag::NPC) changeEntityHealth(attacker, damage.amount / 2);										// IF ATTACKER IS NPC, heal on hit
	damage.lastDamageDealtTime = now;																									// update last damage time
	if (getEntityTag(attacker) == EntityTag::PROJECTILE) deactivateProjectile(attacker);												// IF FOUND, deactivate projectile
	if (getEntityTag(attacker) == EntityTag::HEALTH) destroyEntity(attacker);															// IF HEALTH PICKUP, destroy entity
	return;																																// return
}

void MyEngineSystem::updateAnimationStates(Component& com, float deltaTime)
{
	for (auto& animStateComp : com.animationStates) {														// FOR EACH ANIMATION STATE
		Entity entity = animStateComp.first;																// get entity
		if (!isValidComponent(entity, com.velocities)) continue;											// IF NO VELOCITY, skip
		if (!isValidComponent(entity, com.transforms)) continue;											// IF NO TRANSFORM, skip
		if (isValidComponent(entity, com.dying)) if (com.dying[entity]) continue;							// IF DYING, skip
		if (!isValidComponent(entity, com.animations)) continue;											// IF NO ANIMATION, skip
		Animation& animation = com.animations[entity];														// get Animation
		AnimationState& animState = animStateComp.second;													// get animation state
		Transform& transform = com.transforms[entity];														// get Transform
		const Velocity& velocity = com.velocities[entity];													// get velocity
		std::string newAnim;																				// new animation name
		if (velocity.x == 0 && velocity.y == 0) 															// IF NOT MOVING
			if (animState.previousAnimation.find("_up") != std::string::npos)								// IF UP
				newAnim = animState.idle_up;																// set idle up
			else if (animState.previousAnimation.find("_right") != std::string::npos)						// IF RIGHT
				newAnim = animState.idle_right;																// set idle right
			else if (animState.previousAnimation.find("_down") != std::string::npos)						// IF DOWN
				newAnim = animState.idle_down;																// set idle down
			else newAnim = animState.idle_down;																// default to idle down
		else {																								// ELSE IF MOVING
			if (std::abs(velocity.x) > std::abs(velocity.y)) {												// IF HORIZONTAL MOVEMENT DOMINANT
				newAnim = animState.walk_right;																// use right animation
				if (velocity.x > 0) 																		// IF MOVING RIGHT
					if (!transform.initialFlipH) transform.flipH = false;									// IF NO INITIAL FLIP, no flip
					else transform.flipH = true;															// ELSE INTIAL FLIP, flip
				else 																						// ELSE MOVING LEFT
					if (!transform.initialFlipH) transform.flipH = true;									// IF NO INITIAL FLIP, flip
					else transform.flipH = false;															// ELSE INTIAL FLIP, no flip
			}
			else 																							// ELSE VERTICAL MOVEMENT DOMINANT
				if (velocity.y > 0) newAnim = animState.walk_down;											// IF MOVING DOWN - use down animation
				else newAnim = animState.walk_up;															// ELSE MOVING UP - use up animation
			animState.previousAnimation = newAnim;															// store previous animation
		}
		if (animation.name == newAnim) continue;															// IF SAME ANIMATION, skip
		animation.name = newAnim;																			// set new animation name
		animation.currentFrame = {};																		// reset current frame
		animation.animTimer = {};																			// reset timer
		animation.frameDuration = 0.1f;																		// set frame duration
		animation.loop = true;																				// set loop to true
		animation.frameCount = 1;																			// set frame count to 1
		if (!isValidComponent(entity, com.sprites)) continue; 												// IF SPRITE FOUND
		auto foundSprite = loadedSprites.find(newAnim);														// find Sprite by animation name
		animation.loop = foundSprite->second.loop;															// set loop from sprite
		animation.frameCount = foundSprite->second.frameCount;												// set frame count from sprite
	}
}

void MyEngineSystem::animationSystem(Component& com, float deltaTime)
{
	for (auto& foundAnim : com.animations) {																// FOR EACH ANIMATION
		Entity entity = foundAnim.first;																	// get entity
		Animation& anim = foundAnim.second;																	// get animation
		float frameDur = anim.frameDuration;																// frame duration
		anim.animTimer += deltaTime;																		// increase timer
		if (frameDur <= 0.0f) frameDur = 0.1f;																// IF FRAME DURATION <= 0, set to default
		while (anim.animTimer >= frameDur && frameDur > 0.0f) {												// WHILE TIME TO ADVANCE FRAME
			anim.animTimer -= frameDur;																		// decrease timer
			anim.currentFrame++;																			// increment frame
			if (anim.currentFrame >= anim.frameCount) 														// IF PAST LAST FRAME
				if (anim.loop) anim.currentFrame = {};														// IF LOOPING, reset to first frame
				else anim.currentFrame = anim.frameCount - 1;												// ELSE stay on last frame
		}
	}
}

void MyEngineSystem::processPendingDeaths()
{
	std::vector<Entity> toCheck;																			// entities to check
	toCheck.reserve(component.dying.size());																// reserve space
	for (auto& entry : component.dying) if (entry.second) toCheck.push_back(entry.first);					// collect dying entities
	for (Entity entity : toCheck) {																			// FOR EACH ENTITY TO CHECK
		if (!isValidComponent(entity, component.animations)) { finaliseDeath(entity); continue; }			// IF NO ANIMATION, finalise death
		Animation& anim = component.animations[entity];														// get animation
		if (!anim.loop && anim.frameCount > 0 && anim.currentFrame >= anim.frameCount - 1)					// IF NOT LOOPING AND ANIMATION ENDED
			finaliseDeath(entity);																			// finalize death
	}
}

void MyEngineSystem::finaliseDeath(Entity entity)
{
	component.dying.erase(entity);																			// unmark dying
	if (getEntityTag(entity) == EntityTag::PC) {															// IF PLAYER CHARACTER
		if (isValidComponent(entity, component.transforms)) {												// IF NO TRANSFORM, return
			Transform& transform = component.transforms[entity];											// get transform
			transform.position = transform.startPosition;													// reset position
			transform.newPosition = transform.position;														// reset new position
		}
		if (isValidComponent(entity, component.healths)) {													// IF HAS HEALTH COMPONENT
			Health& health = component.healths[entity];														// get health component
			health.currentHealth = health.maxHealth;														// reset health
		}
		return;																								// return without destroying player
	}
	if (getEntityTag(entity) == EntityTag::NPC) {															// IF NPC
		int npcScoreValue = DEFAULT_NPC_SCORE_VALUE;														// default score value
		if (isValidComponent(entity, component.scores)) npcScoreValue = component.scores[entity].amount;	// IF HAS SCORE COMPONENT, get score value
		score += npcScoreValue;																				// increase score
		destroyEntity(entity);																				// destroy entity
	}
}

void MyEngineSystem::render(std::shared_ptr<GraphicsEngine> gfx)
{
	if (!gfx) return;																											// IF NO GRAPHICS ENGINE, return
	updateCamera(gfx);																											// update camera position
	renderTiles(gfx);																											// render background tiles first, cheap way to handle layers
	struct RenderItem { Entity entity; Sprite* sprite; Transform* transform; Animation* anim; int layer; };						// render item (with layer)
	std::vector<RenderItem> list;																								// list of render items
	list.reserve(component.sprites.size());																						// reserve space from sprite count
	for (auto& spriteComp : component.sprites) {																				// FOR EACH SPRITE COMPONENT
		Entity entity = spriteComp.first;																						// get entity by the first part of the pair
		Sprite& sprite = spriteComp.second;																						// get sprite by the second part of the pair
		if (!isValidComponent(entity, component.transforms)) continue; 															// IF NO TRANSFORM, skip
		Transform& transform = component.transforms[entity];																	// get transform
		if (!transform.active) continue;																						// IF NOT ACTIVE, skip
		Animation* anim = nullptr;																								// animation pointer
		if (isValidComponent(entity, component.animations)) anim = &component.animations[entity];								// IF HAS ANIMATION, get pointer
		list.push_back({ entity, &sprite, &transform, anim, transform.layer });													// add to render list with layer	
	}
	std::stable_sort(list.begin(), list.end(), [](const RenderItem& a, const RenderItem& b) {									// sort render list
		if (a.layer != b.layer) return a.layer < b.layer;																		// IF LAYERS DIFFER, sort by layer
		return a.transform->position.y < b.transform->position.y;																// return by Y position
		});
	for (auto& rendered : list) {																								// FOR EACH RENDER ITEM
		Sprite* spritePtr = rendered.sprite;																					// default sprite pointer
		int currentFrame = {};																									// zero intialise current frame
		if (rendered.anim) {																									// IF HAS ANIMATION
			if (loadedSprites.find(rendered.anim->name) != loadedSprites.end())													// IF SPRITE FOUND BY ANIMATION NAME
				spritePtr = &loadedSprites[rendered.anim->name];																// set sprite pointer to that sprite
			currentFrame = rendered.anim->currentFrame;																			// get current frame from animation
		}
		Sprite& sprite = *spritePtr;																							// get sprite reference
		int frameIndex = sprite.startFrame + currentFrame;																		// calculate frame index
		int columns = 1;																										// default columns
		if (sprite.frameW > 0) {																								// IF FRAME WIDTH > 0
			columns = sprite.textureWidth / sprite.frameW;																		// calculate columns
			if (columns <= 0) columns = 1;																						// prevent division by zero
		}
		int xPos = (frameIndex % columns) * sprite.frameW;																		// get X position in texture sprite sheet
		SDL_Rect src = { xPos, 0, sprite.frameW, sprite.frameH };																// source rectangle
		int width = roundToInt(sprite.frameW * rendered.transform->scale);														// scaled width
		int height = roundToInt(sprite.frameH * rendered.transform->scale);														// scaled height
		int posX = roundToInt(rendered.transform->position.x - cameraPosition.x);												// screen X
		int posY = roundToInt(rendered.transform->position.y - cameraPosition.y);												// screen Y
		SDL_Rect dst = { posX, posY, width, height };																			// destination rectangle
		setEntityColliderRect(rendered.entity, rendered.transform->position.x, rendered.transform->position.y, width, height);	// set collider rect
		SDL_RendererFlip flip = SDL_FLIP_NONE;																					// no flip
		if (rendered.transform->flipH) flip = SDL_FLIP_HORIZONTAL;																// horizontal flip
		double angle = {};																										// zero intialise angle
		if (getEntityTag(rendered.entity) == EntityTag::PROJECTILE) {															// IF PROJECTILE
			if (isValidComponent(rendered.entity, component.velocities)) {														// IF HAS VELOCITY
				Velocity& velocity = component.velocities[rendered.entity];														// get velocity
				angle = std::atan2(velocity.y, velocity.x) * (180.0 / M_PI) - 90.0;												// calculate angle in degrees
			}
		}
		gfx->drawTexture(sprite.texture, &src, &dst, angle, nullptr, flip);														// draw texture
		int healthBarposY = posY - (height / 2);																				// adjust posY for bar rendering
		renderHealthBar(gfx, rendered.entity, posX, healthBarposY, sprite.frameW, sprite.frameH);								// render health bar
	}
}

void MyEngineSystem::renderHealthBar(std::shared_ptr<GraphicsEngine> gfx, Entity entity, int posX, int posY, int width, int height)
{
	if (!isValidComponent(entity, component.healthBars)) return;																	// IF NO HEALTH BAR, return
	if (!isValidComponent(entity, component.healths)) return;																		// IF NO HEALTH, return
	const HealthBar& healthBar = component.healthBars[entity];																		// get health bar
	const Health& health = component.healths[entity];																				// get health
	float percent = float(health.currentHealth) / float(health.maxHealth);															// calculate percentage
	gfx->setDrawColor({ 255, 0, 0, 255 });																							// set draw color to red
	gfx->fillRect(posX, posY, width, BAR_HEIGHT);				 																	// draw background
	SDL_Color fgColor = { 0, 255, 0, 255 };																							// foreground color green
	gfx->setDrawColor(fgColor);																										// set draw color to foreground color
	int fillW = roundToInt(width * percent);																						// calculate fill width
	gfx->fillRect(posX, posY, fillW, BAR_HEIGHT);																					// draw foreground
}

void MyEngineSystem::loadSprite(const std::string& name, const std::string& filename, int frameW, int frameH, int frames, int startFrame, bool loop, float scale, SDL_Color transparent)
{
	if (loadedSprites.count(name)) return;																	// IF SPRITE ALREADY LOADED, return
	SDL_Texture* texture = ResourceManager::loadTexture(filename, transparent);								// load texture
	if (!texture) return;																					// IF FAILED TO LOAD TEXTURE, return
	int width = {}, height = {};																			// zero initialise width and height
	SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);											// query texture 
	Sprite sprite;																							// create Sprite
	sprite.texture = texture;																				// set texture
	sprite.frameW = frameW;																					// set frame width
	sprite.frameH = frameH;																					// set frame height
	sprite.frameCount = frames;																				// set frame count
	sprite.textureWidth = width;																			// set texture width to width which is queried
	sprite.textureHeight = height;																			// set texture height to height which is queried
	sprite.startFrame = startFrame;																			// set start frame
	sprite.loop = loop;																						// set loop
	sprite.scale = scale;																					// set scale
	loadedSprites[name] = std::move(sprite);																// store Sprite
}

void MyEngineSystem::attachSprite(Entity entity, const std::string& spriteName)
{
	auto sprite = loadedSprites.find(spriteName);															// find Sprite
	if (sprite == loadedSprites.end()) return;																// IF SPRITE NOT FOUND, return
	component.sprites[entity] = sprite->second;																// attach sprite
	if (!isValidComponent(entity, component.animations)) {													// IF NO ANIMATION
		Animation anim;																						// create Animation
		anim.name = spriteName;																				// set name
		anim.loop = true;																					// set loop
		anim.frameCount = sprite->second.frameCount;														// set frame count
		anim.frameDuration = 0.1f;																			// set frame duration
		anim.animTimer = {};																				// zero initialise timer
		anim.currentFrame = {};																				// zero initialise current frame
		component.animations[entity] = std::move(anim);														// store Animation
	}
	else {																									// ELSE ANIMATION EXISTS
		Animation& anim = component.animations[entity];														// get Animation
		anim.name = spriteName;																				// set name
		anim.frameCount = sprite->second.frameCount;														// set frame count
		anim.animTimer = {};																				// zero initialise timer
		anim.currentFrame = {};																				// zero initialise current frame
	}
}

void MyEngineSystem::setEntityInput(Entity e, float x, float y)
{
	if (x < -1.0f) x = -1.0f; else if (x > 1.0f) x = 1.0f;													// clamp input
	if (y < -1.0f) y = -1.0f; else if (y > 1.0f) y = 1.0f;													// clamp input
	component.inputs[e] = Input{ x, y };																	// set input
}
void MyEngineSystem::addGroundTile(const std::string& spriteName, int x, int y)
{
	Tile tile;																								// create tile
	tile.x = x; tile.y = y; tile.spriteName = spriteName;													// set tile properties
	groundTiles.push_back(std::move(tile));																	// add tile to ground tiles
}

void MyEngineSystem::renderTiles(std::shared_ptr<GraphicsEngine> gfx) {
	if (!gfx) return;																						// IF NO GRAPHICS ENGINE, return
	Dimension2i window = gfx->getCurrentWindowSize();														// get window size
	for (const Tile& tile : groundTiles) {																	// FOR EACH TILE
		if (loadedSprites.find(tile.spriteName) == loadedSprites.end()) continue;							// IF SPRITE NOT FOUND, skip
		const Sprite& sprite = loadedSprites[tile.spriteName];												// get Sprite
		int screenX = roundToInt(tile.x - cameraPosition.x);												// screen X
		int screenY = roundToInt(tile.y - cameraPosition.y);												// screen Y
		SDL_Rect src = { 0, 0, sprite.frameW, sprite.frameH };												// source rectangle
		SDL_Rect dst = { screenX, screenY, sprite.frameW, sprite.frameH };									// destination rectangle
		gfx->drawTexture(sprite.texture, &src, &dst, 0.0, nullptr, SDL_FLIP_NONE);							// draw texture
	}
}

void MyEngineSystem::changeEntityHealth(Entity entity, int amount) {
	if (!isValidComponent(entity, component.healths)) return;												// IF NO HEALTH COMPONENT, return
	Health& health = component.healths[entity];																// get health
	health.currentHealth += amount;																			// change health
	if (health.currentHealth <= 0) handleDeath(entity, health);												// clamp to 0
	else if (health.currentHealth > health.maxHealth) health.currentHealth = health.maxHealth;				// clamp to max health

}

void MyEngineSystem::handleDeath(Entity entity, Health& health) {
	health.currentHealth = 0;																				// set health to 0
	if (isValidComponent(entity, component.dying)) if (component.dying[entity]) return;						// IF ALREADY DYING, return
	component.dying[entity] = true;																			// mark as dying
	if (!isValidComponent(entity, component.animationStates)) return;										// IF NO ANIMATION STATE, return
	AnimationState& componentState = component.animationStates[entity];										// get animation state
	std::string previousAnim = componentState.previousAnimation;											// previous animation
	std::string newAnim = {};																				// new animation name
	if (!previousAnim.empty()) {																			// IF HAS PREVIOUS ANIMATION
		if (previousAnim.find("_up") != std::string::npos) newAnim = componentState.death_up;				// IF UP, set death up
		else if (previousAnim.find("_right") != std::string::npos) newAnim = componentState.death_right;	// IF RIGHT, set death right
		else if (previousAnim.find("_down") != std::string::npos) newAnim = componentState.death_down;		// IF DOWN, set death down
		else newAnim = componentState.death_down;															// default to death down
	}
	if (!isValidComponent(entity, component.animations)) return;											// IF NO ANIMATION, return
	Animation& anim = component.animations[entity];															// get animation
	anim.name = newAnim;																					// set new animation name
	anim.animTimer = {};																					// reset timer
	anim.loop = false;																						// set no loop
	anim.currentFrame = {};																					// reset current frame
	if (anim.frameDuration <= 0.0f) anim.frameDuration = 0.1f;												// IF FRAME DURATION <= 0, set to default to avoid division by zero
	auto foundSprite = loadedSprites.find(newAnim);															// find sprite
	if (foundSprite != loadedSprites.end())																	// IF FOUND
	{
		anim.frameCount = foundSprite->second.frameCount;													// set frame count from sprite
		anim.loop = foundSprite->second.loop;																// set loop from sprite
	}
	else anim.frameCount = 1;																				// ELSE set frame count to 1
	return;																									// ELSE no animation, return
}

void MyEngineSystem::loadSound(const std::string& name, const std::string& filename) {
	if (loadedSounds.count(name)) return;																	// already loaded
	Mix_Chunk* chunk = ResourceManager::loadSound(filename);												// load sound
	if (!chunk) return;																						// failed to load
	loadedSounds[name] = chunk;																				// store sound
}

void MyEngineSystem::playAudio(const std::string& name, int volume, int loops, int channel)
{
	auto sound = loadedSounds.find(name);																	// find sound from map
	Mix_Chunk* chunk = nullptr;																				// sound chunk pointer
	if (sound != loadedSounds.end()) chunk = sound->second;													// IF FOUND, get chunk
	else {																									// ELSE NOT FOUND
		chunk = ResourceManager::loadSound(name);															// try to load sound
		if (!chunk) return;																					// IF FAILED TO LOAD, return
		loadedSounds[name] = chunk;																			// store sound
	}
	if (volume >= 0) {																						// IF VOLUME SPECIFIED
		int vol = volume;																					// set volume
		if (vol < 0) vol = 0;																				// clamp to 0
		if (vol > MIX_MAX_VOLUME) vol = MIX_MAX_VOLUME;														// clamp to max
		Mix_VolumeChunk(chunk, vol);																		// set chunk volume
	}
	int playChannel = Mix_PlayChannel((channel < 0) ? -1 : channel, chunk, loops);							// play sound
}

void MyEngineSystem::initProjectilePool(Entity owner, size_t poolSize)
{
	if (isValidComponent(owner, component.projectiles)) return;												// IF POOL ALREADY EXISTS, return
	int damage = DEFAULT_UNIT_DAMAGE;																		// default damage
	if (isValidComponent(owner, component.damages)) damage = component.damages[owner].amount;				// IF OWNER HAS DAMAGE COMPONENT, get damage amount
	std::vector<Entity> pool;																				// create pool vector
	pool.reserve(poolSize);																					// reserve space
	for (size_t i = 0; i < poolSize; ++i) {																	// FOR EACH PROJECTILE 
		Entity entity = createEntity();																		// create entity
		pool.push_back(entity);																				// add to pool
		attachSprite(entity, "bullet");
		auto foundSprite = loadedSprites.find("bullet");													// find block sprit
		int width = TILE_SIZE, height = TILE_SIZE;															// default dimensions
		float scale = DEFAULT_ENTITY_SCALE;																	// default dimensions
		scale = foundSprite->second.scale;																	// get scale
		width = foundSprite->second.frameW * roundToInt(scale);												// get width
		height = foundSprite->second.frameH * roundToInt(scale);											// get height
		addComponentProjectileTag(entity, owner);															// add projectile tag component
		addComponentTransform(entity, Vector2f(OFFSCREEN_X, OFFSCREEN_Y), scale);							// add transform component
		addComponentCollider(entity, OFFSCREEN_X, OFFSCREEN_Y, width, height);								// add collider component
		addComponentDamage(entity, damage);																	// add damage component
		addComponentVelocity(entity, 0.0f, 0.0f);															// add velocity component
		addComponentSpeed(entity, DEFAULT_PROJECTILE_SPEED);												// add speed component
	}
	projectilePools[owner] = std::move(pool);																// store pool
}

void MyEngineSystem::fireProjectile(Entity owner, const Vector2f& startPos, const Vector2f& targetPos)
{
	if (!isValidComponent(owner, component.ammos)) return;													// IF NO AMMO COMPONENT, return
	Ammo& ammo = component.ammos[owner];																	// get ammo component
	if (ammo.currentAmmo <= 0) return; 																		// IF NO AMMO, return
	if (now - ammo.lastFireTime < STAT_CHANGE_COOLDOWN) return;												// IF COOLDOWN NOT PASSED, return
	auto foundPool = projectilePools.find(owner);															// find projectile pool
	if (foundPool == projectilePools.end()) return;															// IF NO POOL, return
	std::vector<Entity>& pool = foundPool->second;															// get pool reference
	for (Entity entity : pool) {																			// FOR EACH PROJECTILE IN POOL
		if (!isValidComponent(entity, component.projectiles)) continue;										// IF NO PROJECTILE COMPONENT, skip
		if (!isValidComponent(entity, component.transforms)) continue;										// IF NO TRANSFORM COMPONENT, skip
		if (!isValidComponent(entity, component.velocities)) continue;										// IF NO VELOCITY COMPONENT, skip
		if (!isValidComponent(entity, component.speeds)) continue;											// IF NO SPEED COMPONENT, skip
		Transform& transform = component.transforms[entity];												// get projectile transform
		if (!transform.active) {																			// IF INACTIVE
			ProjectileTag& projectile = component.projectiles[entity];										// get projectile component
			Velocity& velocity = component.velocities[entity];												// get velocity component
			ammo.currentAmmo -= 1;																			// reduce ammo
			ammo.lastFireTime = now;																		// update last fire time
			transform.active = true;																		// set active
			transform.position = startPos;																	// set start position
			transform.newPosition = startPos;																// update new position
			float dx = targetPos.x - startPos.x;															// delta x
			float dy = targetPos.y - startPos.y;															// delta y
			float length = std::sqrt(dx * dx + dy * dy);													// length
			if (length > 0.0f) { dx /= length; dy /= length; }												// IF LENGTH > 0, normalise
			velocity.x = dx * component.speeds[entity].value;												// set velocity x
			velocity.y = dy * component.speeds[entity].value;												// set velocity y
			if (isValidComponent(owner, component.audios))													// IF OWNER HAS AUDIO COMPONENT
				playAudio(component.audios[owner].attackingSound, DEFAULT_SFX_VOLUME);						// play sound
			break;																							// exit loop after firing one projectile
		}
	}
}

void MyEngineSystem::deactivateProjectile(Entity entity)
{
	if (getEntityTag(entity) != EntityTag::PROJECTILE) return;												// IF NOT A PROJECTILE, return
	component.velocities[entity] = Velocity{ 0.0f, 0.0f };													// reset velocity
	Vector2f startPos = component.transforms[entity].startPosition;											// get start position
	component.transforms[entity].position = Vector2f(startPos.x + cameraPosition.x, startPos.y + cameraPosition.y);	// move off-screen
	component.transforms[entity].newPosition = component.transforms[entity].position;						// keep newPosition in sync
	component.transforms[entity].rotation = 0;																// reset rotation
	component.transforms[entity].active = false;															// deactivate projectile
}

void MyEngineSystem::updateCamera(std::shared_ptr<GraphicsEngine> gfx, float deltaTime)
{
	if (!gfx) return;																						// IF NO GRAPHICS ENGINE, return
	Dimension2i window = gfx->getCurrentWindowSize();														// get window size
	Vector2f target;																						// desired camera position
	if (component.players.empty()) {																		// IF NO PLAYER, center camera in world
		target.x = float(worldWidth) * 0.5f - float(window.w) * 0.5f;										// center x
		target.y = float(worldHeight) * 0.5f - float(window.h) * 0.5f;										// center y
	}
	else {																									// ELSE HAS PLAYER
		Entity player = component.players.begin()->first;													// get first player entity
		if (!isValidComponent(player, component.transforms)) return;										// IF NO TRANSFORM, return
		const Transform& transform = component.transforms[player];											// get transform
		target.x = transform.position.x - float(window.w) * 0.5f;											// center x on player
		target.y = transform.position.y - float(window.h) * 0.5f;											// center y on player
	}
	float minX = std::min(0.0f, float(worldWidth) - float(window.w));										// min X
	float maxX = std::max(0.0f, float(worldWidth) - float(window.w));										// max X
	float minY = std::min(0.0f - DEFAULT_FONT_SIZE * 2, float(worldHeight) - float(window.h));				// min Y (account for HUD)
	float maxY = std::max(0.0f, float(worldHeight) - float(window.h) + DEFAULT_FONT_SIZE * 2);				// max Y (account for HUD)
	if (target.x < minX) target.x = minX; if (target.x > maxX) target.x = maxX;								// clamp X
	if (target.y < minY) target.y = minY; if (target.y > maxY) target.y = maxY;								// clamp Y
	cameraPosition.x += (target.x - cameraPosition.x) * cameraSmoothing * deltaTime;						// smooth camera x
	cameraPosition.y += (target.y - cameraPosition.y) * cameraSmoothing * deltaTime;						// smooth camera y
}

void MyEngineSystem::increaseAmmo(Entity ammoPickup, Entity owner) {
	if (!isValidComponent(owner, component.ammos)) return;
	Ammo& ammo = component.ammos[owner];																	// IF AMMO COMPONENT VALID
	if (ammo.currentAmmo >= ammo.maxAmmo) return;															// IF AMMO IS MAX, return
	ammo.currentAmmo += 10;																					// increase ammo
	if (ammo.currentAmmo > ammo.maxAmmo) ammo.currentAmmo = ammo.maxAmmo;									// clamp to max ammo
	if (isValidComponent(ammoPickup, component.audios)) {													// IF AMMO PICKUP HAS AUDIO COMPONENT
		Audio& audio = component.audios[ammoPickup];														// get audio component
		playAudio(audio.attackingSound, DEFAULT_SFX_VOLUME);												// play sound
	}
	destroyEntity(ammoPickup);																				// destroy ammo pickup entity
}

void MyEngineSystem::addComponentIdleAnimations(Entity entity, std::string idle_down, std::string idle_right, std::string idle_up) {
	AnimationState& animState = component.animationStates[entity];											// get animation state
	animState.idle_down = idle_down;																		// set idle down
	animState.idle_right = idle_right;																		// set idle right
	animState.idle_up = idle_up;																			// set idle up
	animState.previousAnimation = idle_down;																// set previous animation
}

void MyEngineSystem::addComponentWalkAnimations(Entity entity, std::string walk_down, std::string walk_right, std::string walk_up) {
	AnimationState& animState = component.animationStates[entity];											// get animation state
	animState.walk_down = walk_down;																		// set walk down
	animState.walk_right = walk_right;																		// set walk right
	animState.walk_up = walk_up;																			// set walk up
}

void MyEngineSystem::addComponentDeathAnimations(Entity entity, std::string death_down, std::string death_right, std::string death_up) {
	AnimationState& animState = component.animationStates[entity];											// get animation state
	animState.death_down = death_down;																		// set death down
	animState.death_right = death_right;																	// set death right
	animState.death_up = death_up;																			// set death up
}

void MyEngineSystem::addComponentAudio(Entity entity, std::string damageSound, std::string attackingSound) {
	Audio& audio = component.audios[entity];																// create audio component
	audio.damageSound = damageSound;																		// set damage sound
	audio.attackingSound = attackingSound;																	// set attack sound
}

MyEngineSystem::EntityTag MyEngineSystem::getEntityTag(Entity entity) {
	if (isValidComponent(entity, component.players)) return EntityTag::PC;									// IF HAS PLAYER COMPONENT, return PC
	if (isValidComponent(entity, component.npcs)) return EntityTag::NPC;									// IF HAS NPC COMPONENT, return NPC
	if (isValidComponent(entity, component.ammoPickups)) return EntityTag::AMMO;							// IF HAS AMMO PICKUP COMPONENT, return AMMO
	if (isValidComponent(entity, component.healthPickups)) return EntityTag::HEALTH;						// IF HAS HEALTH PICKUP COMPONENT, return HEALTH
	if (isValidComponent(entity, component.projectiles)) return EntityTag::PROJECTILE;						// IF HAS PROJECTILE COMPONENT, return PROJECTILE
	if (isValidComponent(entity, component.endLevels)) return EntityTag::ENDLEVEL;							// IF HAS END LEVEL COMPONENT, return ENDLEVEL
	return EntityTag::Unknown;																				// ELSE return Unknown
}

bool MyEngineSystem::isProjectileOwner(Entity entity, Entity other) {
	if (getEntityTag(entity) == EntityTag::PROJECTILE) {													// IF ENTITY IS PROJECTILE
		ProjectileTag& projectile = component.projectiles[entity];											// get projectile
		if (other == projectile.owner) return true;															// IF OTHER IS OWNER, return true
	}
	else if (getEntityTag(other) == EntityTag::PROJECTILE) {												// IF OTHER IS PROJECTILE
		ProjectileTag& projectile = component.projectiles[other];											// get projectile as other
		if (entity == projectile.owner) return true;														// IF ENTITY IS OWNER, return true
	}
	return false;																							// ELSE return false
}

void MyEngineSystem::setEntityColliderRect(Entity entity, float posX, float posY, int width, int height) {
	if (isValidComponent(entity, component.colliders)) {													// IF HAS COLLIDER
		Collider& collider = component.colliders[entity];													// get collider
		collider.rect.x = roundToInt(posX);																	// set x
		collider.rect.y = roundToInt(posY);																	// set y
		collider.rect.w = width;																			// set width
		collider.rect.h = height;																			// set height
	}
}
template <typename T>
bool MyEngineSystem::isValidComponent(Entity entity, ComponentMap<T>& comp) {
	if (comp.find(entity) != comp.end())  return true;														// IF COMPONENT EXISTS, return true
	return false;																							// return false
}

void MyEngineSystem::flushDestroyedEntities()
{
	if (entitiesToDestroy.empty()) return;																	// IF NO ENTITIES TO DESTROY, return
	for (Entity entity : entitiesToDestroy) {																// FOR EACH ENTITY TO DESTROY, erase all components
		component.transforms.erase(entity);
		component.velocities.erase(entity);
		component.sprites.erase(entity);
		component.animations.erase(entity);
		component.players.erase(entity);
		component.npcs.erase(entity);
		component.ammoPickups.erase(entity);
		component.healthPickups.erase(entity);
		component.endLevels.erase(entity);
		component.healths.erase(entity);
		component.colliders.erase(entity);
		component.damages.erase(entity);
		component.speeds.erase(entity);
		component.ammos.erase(entity);
		component.healthBars.erase(entity);
		component.inputs.erase(entity);
		component.animationStates.erase(entity);
		component.dying.erase(entity);
		component.audios.erase(entity);
		component.projectiles.erase(entity);
		component.scores.erase(entity);
		activeEntities.erase(entity);																		// erase from active entities
	}
	entitiesToDestroy.clear();																				// clear destruction list
}

void MyEngineSystem::clearLevelExcept(Entity player)
{
	for (Entity entity : activeEntities)																	// FOR EACH ACTIVE ENTITY
		if (entity != player) entitiesToDestroy.insert(entity);												// mark for destruction if not player
	bool keeperHadPool = (player != 0 && projectilePools.find(player) != projectilePools.end());			// check if player had a projectile pool
	flushDestroyedEntities();																				// flush destroyed entities at this point to avoid issues
	if (player == 0)  projectilePools.clear();																// IF PLAYER IS 0, clear all projectile pools
	else for (auto projectilePool = projectilePools.begin(); projectilePool != projectilePools.end(); )		// ELSE, FOR EACH POOL
		if (projectilePool->first == player) ++projectilePool;												// IF PLAYER, skip
		else projectilePool = projectilePools.erase(projectilePool);										// ELSE ERASE
	groundTiles.clear();																					// clear ground tiles
	cameraPosition = Vector2f{ 0.0f, 0.0f };																// reset camera position
	if (keeperHadPool && isValidComponent(player, component.players))										// IF PLAYER HAD POOL AND IS VALID PLAYER
		initProjectilePool(player, DEFAULT_PROJECTILES_PER_OWNER);											// reinitialise projectile pool
}