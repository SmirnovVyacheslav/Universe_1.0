﻿#pragma once

#include <vector>
#include <Windows.h>

#include <xnamath.h>

struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
	XMFLOAT3 Normal;
};

struct Object
{
	std::wstring shader;
	std::vector<DWORD> indices;
	std::vector<SimpleVertex> vertices;
	int size;
};

class Geometry
{
	Object* person;
	Object* landscape;

	std::vector<Object*> scene;

	std::vector<Object*>::iterator beginIt;
	std::vector<Object*>::iterator endIt;

	int u = 32;
	int v = 32;
	//--------------------------------------------------------------------------------------
	

public:

	Geometry();

	std::vector<Object*>::iterator& begin();

	std::vector<Object*>::iterator& end();
};

class Plane
{
public:
	static void make_plane(Object * obj, XMFLOAT3 pos, int w, int h, int direction, float scale);
};

class Cube
{
public:
	//Cube(XMFLOAT3 pos, int direction, int size) {};

	//static void make_cube(Object *obj, XMFLOAT3 pos, int direction, float size);
};