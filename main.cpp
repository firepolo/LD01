#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.hpp"
#include <vector>
#include <string>
#include <stack>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <random>
#include <chrono>

typedef unsigned short UID;

enum Action : unsigned short
{
	ACTION_PLAYER_LEFT = 1,
	ACTION_PLAYER_RIGHT,
	ACTION_PLAYER_JUMP,
	ACTION_PLAYER_AIM,
	ACTION_PLAYER_START_SHOOT,
	ACTION_PLAYER_STOP_SHOOT,

	ACTION_SPECTATOR_LEFT,
	ACTION_SPECTATOR_RIGHT,
	ACTION_SPECTATOR_UP,
	ACTION_SPECTATOR_BOTTOM
};

class Particle;
class Entity;

class Assets
{
public:
	static olc::Sprite* Spritesheet;
};

class Map
{
public:
	Map();
	~Map();

	Entity* GetEntity(UID uid);
	void SetPlayerID(UID uid);

	void AddParticle(Particle* particle);

	UID AddEntity(Entity* entity);
	void RemoveEntity(UID uid);

	bool IsBlock(const olc::vi2d& position);
	void Update(float elapsedTime);
	void Render(olc::PixelGameEngine* pge);

public:
	static const olc::vi2d SIZE;

private:
	char* blocks;

	std::unordered_map<UID, Entity*> entities;
	std::vector<Particle*> particles;

	UID playerID;
	UID globalID;
};

class Particle
{
public:
	Particle(olc::vf2d position, olc::vf2d velocity, int lifetime);

	bool Update(float elapsedTime);
	void Render(olc::PixelGameEngine* pge, const olc::vf2d& offset);
	
private:
	olc::vf2d position;
	olc::vf2d velocity;
	int lifetime;
};

class Entity
{
public:
	bool IsDead() const;
	olc::vi2d GetPosition() const;
	void SetMap(UID uid, Map* map);

	void Kill();

	virtual void OnAction(Action action, const olc::vf2d& vector = olc::vf2d()) = 0;
	virtual void OnBlockCollision(const olc::vf2d& velocity) = 0;

	virtual void Update(float elapsedTime) = 0;
	virtual void Render(olc::PixelGameEngine* pge, const olc::vf2d& offset) = 0;

protected:
	Entity(const std::array<olc::vi2d, 4>& hitboxes, const olc::vf2d& position, const olc::vf2d& velocity);

	void Move();

	UID uid;
	Map* map;
	bool dead;
	std::array<olc::vi2d, 4> hitboxes;
	olc::vf2d position;
	olc::vf2d velocity;

private:
	void Move(const olc::vf2d& direction, float speed, int start);
	void Move(const olc::vf2d& velocity, int start);
};

class Player : public Entity
{
public:
	Player();

	void OnAction(Action action, const olc::vf2d& vector);
	void OnBlockCollision(const olc::vf2d& velocity);

	void Update(float elapsedTime);
	void Render(olc::PixelGameEngine* pge, const olc::vf2d& offset);

private:
	bool onFloor;
	olc::vf2d aim;
	char shoot;
};

class Bullet : public Entity
{
public:
	Bullet(const olc::vf2d& position, const olc::vf2d& velocity);

	void OnAction(Action action, const olc::vf2d& vector);
	void OnBlockCollision(const olc::vf2d& velocity);

	void Update(float elapsedTime);
	void Render(olc::PixelGameEngine* pge, const olc::vf2d& offset);
};

const olc::vi2d BLOCK_SIZE = { 16, 16 };
const olc::vi2d BLOCK_SIZE_2 = BLOCK_SIZE / 2;

olc::Sprite* Assets::Spritesheet;

const olc::vi2d Map::SIZE = { 32, 16 };

Map::Map() : globalID(0)
{
	blocks = new char[SIZE.x * SIZE.y];

	std::mt19937 rnd(std::chrono::high_resolution_clock::now().time_since_epoch().count());

	for (int y = 0; y < SIZE.y; ++y)
	{
		for (int x = 0; x < SIZE.x; ++x)
		{
			int b = rnd();
			if (b >= 0 && b < 0x22222222) blocks[y * SIZE.x + x] = 2;
			else if (b >= 0x22222222 && b < 0x33333333) blocks[y * SIZE.x + x] = 3;
			else blocks[y * SIZE.x + x] = 0;
		}
	}

	for (int i = 0; i < SIZE.x; ++i) blocks[i] = 2;
	for (int i = 0; i < SIZE.x; ++i) blocks[(SIZE.y - 1) * SIZE.x + i] = 1;
	for (int i = 0; i < SIZE.y - 1; ++i) blocks[i * SIZE.x] = 2;
	for (int i = 0; i < SIZE.y - 1; ++i) blocks[i * SIZE.x + SIZE.x - 1] = 2;
}

Map::~Map()
{
	for (auto& entity : entities) delete entity.second;
	delete[] blocks;
}

Entity* Map::GetEntity(UID uid)
{
	return entities[uid];
}

void Map::SetPlayerID(UID uid)
{
	playerID = uid;
}

void Map::AddParticle(Particle* particle)
{
	particles.push_back(particle);
}

UID Map::AddEntity(Entity* entity)
{
	entity->SetMap(++globalID, this);
	entities.insert({ globalID, entity });
	return globalID;
}

void Map::RemoveEntity(UID uid)
{
	GetEntity(uid)->Kill();
}

bool Map::IsBlock(const olc::vi2d& position)
{
	return position.x < 0 || position.x >= SIZE.x || position.y < 0 || position.y >= SIZE.y || blocks[position.y * SIZE.x + position.x];
}

void Map::Update(float elapsedTime)
{
	for (int i = 0; i < particles.size(); ++i)
	{
		if (particles[i]->Update(elapsedTime))
		{
			delete particles[i];
			particles.erase(particles.begin() + i);
		}
	}

	for (auto it = entities.begin(), end = entities.end(); it != end;)
	{
		auto entity = it++;
		entity->second->Update(elapsedTime);
		if (entity->second->IsDead())
		{
			delete entity->second;
			entities.erase(entity);
		}
	}
}

void Map::Render(olc::PixelGameEngine* pge)
{
	const olc::vi2d centerScreen = olc::vi2d(pge->ScreenWidth() >> 1, pge->ScreenHeight() >> 1);
	const olc::vi2d offset = GetEntity(playerID)->GetPosition() - centerScreen;

	pge->Clear(olc::BLACK);

	pge->SetPixelMode(olc::Pixel::MASK);

	for (int y = 0; y < SIZE.y; ++y)
	{
		const int scan = y * SIZE.x;
		for (int x = 0; x < SIZE.x; ++x) pge->DrawPartialSprite(olc::vi2d(x, y) * BLOCK_SIZE - offset, Assets::Spritesheet, olc::vi2d(blocks[scan + x], 0) * BLOCK_SIZE, BLOCK_SIZE);
	}

	for (auto& entity : entities) entity.second->Render(pge, offset);
	for (auto& particle : particles) particle->Render(pge, offset);

	pge->SetPixelMode(olc::Pixel::NORMAL);
}

Particle::Particle(olc::vf2d position, olc::vf2d velocity, int lifetime) : position(position), velocity(velocity), lifetime(lifetime)
{
}

bool Particle::Update(float elapsedTime)
{
	position += velocity;
	return --lifetime <= 0;
}

void Particle::Render(olc::PixelGameEngine* pge, const olc::vf2d& offset)
{
	pge->Draw(position - offset, olc::DARK_RED);
}

Entity::Entity(const std::array<olc::vi2d, 4>& hitboxes, const olc::vf2d& position, const olc::vf2d& velocity) : hitboxes(hitboxes), position(position), velocity(velocity), dead(false)
{
}

bool Entity::IsDead() const
{
	return dead;
}

olc::vi2d Entity::GetPosition() const
{
	return position;
}

void Entity::SetMap(UID uid, Map* map)
{
	this->uid = uid;
	this->map = map;
}

void Entity::Kill()
{
	dead = true;
}

void Entity::Move()
{
	if (velocity.x <= 0) Move(olc::vf2d(-1, 0), -velocity.x, 3);
	else if (velocity.x >= 0) Move(olc::vf2d(1, 0), velocity.x, 1);
	if (velocity.y <= 0) Move(olc::vf2d(0, -1), -velocity.y, 0);
	else if (velocity.y >= 0) Move(olc::vf2d(0, 1), velocity.y, 2);
}

void Entity::Move(const olc::vf2d& velocity, int start)
{
	olc::vf2d p = position + velocity;

	for (int i = 0; i < 2; ++i)
	{
		if (map->IsBlock((p + hitboxes[(start + i) % hitboxes.size()]) / BLOCK_SIZE))
		{
			OnBlockCollision(velocity);
			return;
		}
	}
	position = p;
}

void Entity::Move(const olc::vf2d& direction, float speed, int start)
{
	int base = speed;
	while (base-- > 0) Move(direction, start);
	Move(direction * (speed - int(speed)), start);
}

Player::Player() : Entity({ olc::vi2d(-3, -4), olc::vi2d(2, -4), olc::vi2d(2, 7), olc::vi2d(-3, 7) }, olc::vf2d(64, 32), olc::vf2d(0, 0)), onFloor(false), shoot(0)
{
}

void Player::OnAction(Action action, const olc::vf2d& vector)
{
	switch (action)
	{
		case ACTION_PLAYER_LEFT:
		{
			velocity.x = -2;
			break;
		}
		case ACTION_PLAYER_RIGHT:
		{
			velocity.x = 2;
			break;
		}
		case ACTION_PLAYER_JUMP:
		{
			if (onFloor) velocity.y = -5;
			break;
		}
		case ACTION_PLAYER_AIM:
		{
			aim = vector;
			break;
		}
		case ACTION_PLAYER_START_SHOOT:
		{
			if (shoot++ % 12 == 0) map->AddEntity(new Bullet(position, aim));
			break;
		}
		case ACTION_PLAYER_STOP_SHOOT:
		{
			shoot = 0;
			break;
		}
	}
}

void Player::OnBlockCollision(const olc::vf2d& velocity)
{
	if (velocity.y > 0)
	{
		onFloor = true;
		this->velocity.y = 0.25;
	}
	else if (velocity.y < 0) this->velocity.y = 0;
}

void Player::Update(float elapsedTime)
{
	onFloor = false;

	Move();

	velocity.x = 0;
	if (!onFloor) velocity.y += 0.25;
}

void Player::Render(olc::PixelGameEngine* pge, const olc::vf2d& offset)
{
	const olc::vi2d pos = position - offset;

	pge->DrawPartialSprite(pos - BLOCK_SIZE_2, Assets::Spritesheet, olc::vi2d(0, 1) * BLOCK_SIZE, BLOCK_SIZE);

	pge->DrawLine(olc::vi2d(pge->ScreenWidth() >> 1, pge->ScreenHeight() >> 1), pos + aim * 10, olc::BLACK);
}

Bullet::Bullet(const olc::vf2d& position, const olc::vf2d& velocity) : Entity({ olc::vi2d(-1, -1), olc::vi2d(0, -1), olc::vi2d(0, 0), olc::vi2d(-1, 0) }, position, velocity * 3)
{
}

void Bullet::OnAction(Action action, const olc::vf2d& vector)
{
}

void Bullet::OnBlockCollision(const olc::vf2d& velocity)
{
	std::mt19937 rnd(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	for (int i = 0; i < 16; ++i) map->AddParticle(new Particle(position, olc::vf2d(float(rnd() / (float)rnd.max()) - float(rnd() / (float)rnd.max()), float(rnd() / (float)rnd.max()) - float(rnd() / (float)rnd.max())), rnd() % 20));
	Kill();
}

void Bullet::Update(float elapsedTime)
{
	Move();
}

void Bullet::Render(olc::PixelGameEngine* pge, const olc::vf2d& offset)
{
	pge->FillRect(position - offset - olc::vi2d(1, 1), olc::vi2d(2, 2), olc::BLACK);
}

class PocketGame : public olc::PixelGameEngine
{
public:
	PocketGame()
	{
		sAppName = "Client";
	}

public:
	bool OnUserCreate() override
	{
		Assets::Spritesheet = new olc::Sprite("./spritesheet.png");

		map = new Map();

		playerID = map->AddEntity(new Player());
		map->SetPlayerID(playerID);

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		Entity* player = map->GetEntity(playerID);

		if (GetKey(olc::Key::UP).bPressed || GetKey(olc::Key::W).bPressed) player->OnAction(ACTION_PLAYER_JUMP);

		if (GetKey(olc::Key::LEFT).bHeld || GetKey(olc::Key::A).bHeld) player->OnAction(ACTION_PLAYER_LEFT);
		else if (GetKey(olc::Key::RIGHT).bHeld || GetKey(olc::Key::D).bHeld) player->OnAction(ACTION_PLAYER_RIGHT);

		if (GetMouse(0).bHeld) player->OnAction(ACTION_PLAYER_START_SHOOT);
		else player->OnAction(ACTION_PLAYER_STOP_SHOOT);

		map->GetEntity(playerID)->OnAction(ACTION_PLAYER_AIM, olc::vf2d(GetMousePos() - olc::vi2d(ScreenWidth() >> 1, ScreenHeight() >> 1)).norm());
		
		map->Update(fElapsedTime);
		map->Render(this);

		return true;
	}

	bool OnUserDestroy() override
	{
		delete map;
		delete Assets::Spritesheet;

		return true;
	}

private:
	Map* map;
	UID playerID;
};

int main(int argc, char *argv[])
{
	PocketGame game;
	if (game.Construct(256, 240, 3, 3, false, true)) game.Start();
	return 0;
}
