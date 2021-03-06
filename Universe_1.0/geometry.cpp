﻿/******************************************************************************
	 * File: geometry.cpp
	 * Description: Contains geometry building.
	 * Created: 09 Aug 2020
	 * Copyright: (C) 2020 Vyacheslav Smirnov, All rights reserved.
	 * Author: Vyacheslav Smirnov
	 * Email: necrolazy@gmail.com

******************************************************************************/

#include "geometry.h"

namespace Geometry
{
	int Object::obj_counter = 0;
	std::vector<Object*> objects;

	Shape::Shape(std::string type, float size)
	{
		if (type == "square")
		{
			wrap = true;
			data = { std::make_pair(size, 0.0f),
					 std::make_pair(size, 90.0f),
					 std::make_pair(size, 180.0f),
					 std::make_pair(size, 270.0f) };
		}
		else if (type == "plane")
		{
			// * - * - * - * - * - * - * - * - *
			//  \   \   \   \  |  /   /   /   /
			//                 *
			//            start point
			wrap = false;
			int segments = 100;
			float normal_size = 1.0f;
			float step = 1.0f / static_cast<float>(segments);

			Math_3d::Vector_3d start_point  = { 0.0f, 0.0f, 0.0f };
			Math_3d::Vector_3d normal_vec   = { 0.0f, 1.0f * normal_size, 0.0f };
			Math_3d::Vector_3d plane_vec    = { 1.0f, 0.0f, 0.0f };
			Math_3d::Vector_3d center_point = start_point + normal_vec;

			for (float t = -0.5f; t <= 0.5f; t += step)
			{
				float plane_length = static_cast<float>(abs(size * t));
				Math_3d::Vector_3d plane_point = center_point + (size * t) * plane_vec;
				Math_3d::Vector_3d bisector_vec = plane_point - start_point;

				float bisector_length = bisector_vec.length();
				float bisector_angle = Math_3d::radian_to_degree(Math_3d::angle(bisector_vec, normal_vec));
				if (t < 0.0f)
				{
					bisector_angle *= -1.0f;
				}
				data.push_back(std::make_pair(bisector_length, bisector_angle));
			}
		}
	}

	std::vector<std::pair<float, float>>::iterator Shape::begin()
	{
		return data.begin();
	}
	std::vector<std::pair<float, float>>::iterator Shape::end()
	{
		return data.end();
	}
	std::vector<std::pair<float, float>>::const_iterator Shape::begin() const
	{
		return data.cbegin();
	}
	std::vector<std::pair<float, float>>::const_iterator Shape::end() const
	{
		return data.cend();
	}

	int Shape::size() const
	{
		return data.size();
	}

	int Shape::get_edges_number() const
	{
		return wrap ? data.size() : data.size() - 1;
	}

	Path::Path(std::vector<Math_3d::Vector_3d> control_points)
	: control_points(control_points) {}

	Math_3d::Vector_3d Path::get_point(float t) const
	{
		Math_3d::Vector_3d result;
		int n = control_points.size() - 1;

		for (int i = 0; i < control_points.size(); ++i)
		{
			float binom_factorial = Math_3d::factorial(n) / (Math_3d::factorial(i) * Math_3d::factorial(n - i));
			float bernstein_polynom = binom_factorial * pow(t, i) * pow(1.0f - t, n - i);

			result += control_points[i] * bernstein_polynom;
		}

		return result;
	}

	Generator::Generator() {}

	Generator::Generator(std::unique_ptr<Path> path, std::unique_ptr<Shape> shape, Math_3d::Vector_3d base_vec)
	: path(std::move(path)), shape(std::move(shape)), base_vec(base_vec) {}

	void Generator::make_mesh(Object_Data& data)
	{
		int object_first_index = data.vertices.size();

		// Go through slices and apply shape to them
		for (float t = 0.0f; t < 1.0f; t += step)
		{
			int start_index = data.vertices.size();
			Math_3d::Vector_3d center = path->get_point(t);

			Math_3d::Vector_3d path_vec = (path->get_point(t + path_delta) - center).normalize();
			base_vec = Math_3d::project_vector_to_plane(base_vec, center, path_vec).normalize();
			for (auto item : *shape)
			{
				Vertex vertex;
				const float length = item.first;
				const float angle = item.second;

				vertex.pos = center + (Math_3d::rotate_vector(base_vec, path_vec, angle).normalize() * length);
				// vertex.normal = (vertex.pos - center).normalize();
				data.vertices.push_back(vertex);
			}

			// Set indices, not required for first
			if (t > path_delta)
			{
				for (int i = 0; i < shape->get_edges_number(); ++i)
				{
					int p1_index = start_index - shape->size() + i;
					int p2_index = start_index - shape->size() + (i + 1) % shape->size();
					int p3_index = start_index + (i + 1) % shape->size();
					int p4_index = start_index + i;

					data.indices.push_back(p1_index);
					data.indices.push_back(p2_index);
					data.indices.push_back(p3_index);

					data.indices.push_back(p1_index);
					data.indices.push_back(p3_index);
					data.indices.push_back(p4_index);
				}
			}
		}
		int object_last_index = data.vertices.size();

		calc_normale(data, object_first_index);

		if (!solid)
		{
			data.size = data.indices.size();
			return;
		}

		Vertex vertex;
		// Begin center vertex
		vertex.pos = path->get_point(0.0);
		data.vertices.push_back(vertex);
		// End center vertex
		vertex.pos = path->get_point(1.0);
		data.vertices.push_back(vertex);

		// Add mesh for begin sector
		Math_3d::Vector_3d normal = (path->get_point(0.0f) - path->get_point(path_delta)).normalize();
		make_solid(data, object_first_index, object_last_index, normal);
		// Add mesh for end sector
		normal = (path->get_point(1.0f) - path->get_point(1.0f - path_delta)).normalize();
		make_solid(data, object_last_index - shape->size(), object_last_index + 1, normal);

		data.size = data.indices.size();
	}

	void Generator::make_solid(Object_Data& data, int abc_start_index, int center_index, Math_3d::Vector_3d normal)
	{
		int a_start_index = 0;
		int b_start_index = split_points + 1;
		int c_start_index = 0;
		for (int i = 2; i <= split_points + 2; ++i)
		{
			c_start_index += i;
		}
		for (int i = 0; i < shape->size(); ++i)
		{
			int a_index = abc_start_index + i;
			int b_index = abc_start_index + (i + 1) % shape->size();
			int c_index = center_index;

			Math_3d::Vector_3d ab_vec = (data.vertices[b_index].pos - data.vertices[a_index].pos);
			Math_3d::Vector_3d bc_vec = (data.vertices[c_index].pos - data.vertices[b_index].pos);
			Math_3d::Vector_3d ac_vec = (data.vertices[c_index].pos - data.vertices[a_index].pos);

			// A * * B
			//  * * *
			//   * *
			//    C
			// Loop via level
			// Add vertex
			int start_index = data.vertices.size();
			int count = 0;
			for (int j = 0; j < split_points + 2; ++j)
			{
				Math_3d::Vector_3d start_point = data.vertices[a_index].pos + ac_vec * sector_step * static_cast<float>(j);
				// Loop via row
				for (int k = 0; k < split_points + 2 - j; k++)
				{
					/*if (count == a_start_index)
					{
						data.vertices.push_back(data.vertices[a_index]);
					}
					else if (count == b_start_index)
					{
						data.vertices.push_back(data.vertices[b_index]);
					}
					else if (count == c_start_index)
					{
						data.vertices.push_back(data.vertices[c_index]);
					}
					else
					{*/
						Math_3d::Vector_3d new_point = start_point + ab_vec * sector_step * static_cast<float>(k);
						Vertex new_vertex;
						new_vertex.pos = new_point;
						new_vertex.normal = normal;
						data.vertices.push_back(new_vertex);
					//}
					count++;
				}
			}

			// Add indexies
			int sub_row_index_start = 0;
			for (int j = 0; j < split_points + 1; ++j)
			{
				for (int k = 0; k < (split_points * 2) + 1 - j * 2; ++k)
				{
					// Add low triangle
					// 1   2
					//   3
					if (!(k % 2))
					{
						int p1_index = start_index + sub_row_index_start + (k / 2);
						int p2_index = start_index + sub_row_index_start + (k / 2) + 1;
						int p3_index = start_index + sub_row_index_start + (split_points + 2 - j) + (k / 2);

						data.indices.push_back(p1_index);
						data.indices.push_back(p2_index);
						data.indices.push_back(p3_index);
					}
					// Add high triangle
					//   1
					// 2   3
					else
					{
						int p1_index = start_index + sub_row_index_start + (k / 2) + 1;
						int p2_index = start_index + sub_row_index_start + (split_points + 2 - j) + (k / 2);
						int p3_index = start_index + sub_row_index_start + (split_points + 2 - j) + (k / 2) + 1;

						data.indices.push_back(p1_index);
						data.indices.push_back(p2_index);
						data.indices.push_back(p3_index);
					}
				}
				sub_row_index_start += split_points + 2 - j;
			}
		}
	}

	void Generator::calc_normale(Object_Data& data, int start_index)
	{
		int n_steps = static_cast<int>(1.0f / step);
		int curr_index, up_index, down_index, left_index, right_index;
		// Calc normales
		//      *      - up_index   -    *
		//                  |
		//  left_index - curr_index - right_index
		//                  |
		//      *      - down_index -    *

		for (int i = 0; i < n_steps; ++i)
		{
			for (int j = 0; j < shape->size(); ++j)
			{
				curr_index = start_index + i * shape->size() + j;
				up_index = start_index + (i - 1 < 0 ? i + 1 : i - 1) * shape->size() + j;
				down_index = start_index + (i + 1 == n_steps ? i - 1 : i + 1) * shape->size() + j;
				// If wrap
				if (shape->size() == shape->get_edges_number())
				{
					left_index = start_index + i * shape->size() + (j - 1 < 0 ? shape->size() - 1 : j - 1);
					right_index = start_index + i * shape->size() + (j + 1) % shape->size();
				}
				else
				{
					left_index = start_index + i * shape->size() + (j - 1 < 0 ? j + 1 : j - 1);
					right_index = start_index + i * shape->size() + (j + 1 == shape->size() ? j - 1 : j + 1);
				}

				Math_3d::Vector_3d path_normal = (data.vertices[curr_index].pos -
												  path->get_point(static_cast<float>(i) * step)).normalize();

				Math_3d::Vector_3d u_vec = data.vertices[up_index].pos - data.vertices[curr_index].pos;
				Math_3d::Vector_3d d_vec = data.vertices[down_index].pos - data.vertices[curr_index].pos;
				Math_3d::Vector_3d l_vec = data.vertices[left_index].pos - data.vertices[curr_index].pos;
				Math_3d::Vector_3d r_vec = data.vertices[right_index].pos - data.vertices[curr_index].pos;

				auto check_normale = [path_normal](Math_3d::Vector_3d vec) -> Math_3d::Vector_3d
				{
					if (Math_3d::radian_to_degree(Math_3d::angle(path_normal, vec)) > 90.0f)
					{
						return vec * -1.0f;
					}
					return vec;
				};

				Math_3d::Vector_3d normale_1 = check_normale(u_vec ^ l_vec);
				Math_3d::Vector_3d normale_2 = check_normale(u_vec ^ r_vec);
				Math_3d::Vector_3d normale_3 = check_normale(d_vec ^ l_vec);
				Math_3d::Vector_3d normale_4 = check_normale(d_vec ^ r_vec);

				data.vertices[curr_index].normal = (normale_1 + normale_2 + normale_3 + normale_4).normalize();
				data.vertices[curr_index].normal = check_normale(data.vertices[curr_index].normal);
			}
		}
	}

	Geometry::Geometry()
	{
		person = new Person;
		person->create();
		scene.push_back(person);

		landscape = new Landscape;
		landscape->create();
		scene.push_back(landscape);
	}

	Geometry::~Geometry()
	{
		for (auto obj : scene)
		{
			delete obj;
		}
	}

	std::vector<Object*>::iterator Geometry::begin()
	{
		return objects.begin();
	}

	std::vector<Object*>::iterator Geometry::end()
	{
		return objects.end();
	}


	Person::Person(Object* base) : Object(base)
	{
		data = new Object_Data;
		objects.push_back(this);
	}

	Person::~Person()
	{
		delete data;
	}

	void Person::create()
	{
		std::vector<Math_3d::Vector_3d> control_points = { 
			Math_3d::Vector_3d(0.0f, -2.0f, 0.0f),
			//Math_3d::Vector_3d(1.0f, 2.0f, -1.0f),
			Math_3d::Vector_3d(0.0f, 1.0f, 0.0f) };
		Math_3d::Vector_3d base_vec = Math_3d::Vector_3d(1.0f, 0.0f, 0.0f);
		Generator mesh_generator(std::make_unique<Path>(control_points),
								 std::make_unique<Shape>(std::string("square"), 3.0f), base_vec);

		mesh_generator.make_mesh(*data);

		data->color = { 0.6f, 0.3f, 0.0f };
	}


	Landscape::Landscape(Object* base) : Object(base)
	{
		data = new Object_Data;
		objects.push_back(this);
	}

	Landscape::~Landscape()
	{
		delete data;
	}

	void Landscape::create()
	{
		std::vector<Math_3d::Vector_3d> control_points = {
			Math_3d::Vector_3d(-50.0f, -5.0f, 0.0f),
			Math_3d::Vector_3d(50.0f, -5.0f, 0.0f) };
		Math_3d::Vector_3d base_vec = Math_3d::Vector_3d(0.0f, 1.0f, 0.0f);
		Generator mesh_generator(std::make_unique<Path>(control_points),
			std::make_unique<Shape>(std::string("plane"), 100.0f), base_vec);

		mesh_generator.make_mesh(*data);

		data->color = { 0.0f, 0.3f, 0.4f };
	}


	Object::Object(Object* base) : base(base)
	{
		data = nullptr;
		id = obj_counter++;

		pos = { 0.0f, 0.0f, 0.0f };
	}

	Object::~Object() {}

	int Object::get_id()
	{
		return id;
	}

	Object_Data* Object::get_data()
	{
		return data;
	}


	void Object::move_down()
	{
		if (data != nullptr)
		{
			for (Vertex& vertex : data->vertices)
			{
				vertex.pos.y -= 10.0f;
			}
		}
		else
		{
			for (auto obj : components)
			{
				obj->move_down();
			}
		}
	}
}
