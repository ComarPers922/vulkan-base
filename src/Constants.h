#pragma once

#include "lib.h"

Triangle_Mesh quad = {
	{
		{{ -1.f, -1.f, 0.f }, { 0.f, 0.f }},
		{{ 1.f, -1.f, 0.f }, { 1.f, 0.f }},
		{{ -1.f, 1.f, 0.f }, { 0.f, 1.f }},
		{{ 1.f, 1.f, 0.f }, { 1.f, 1.f }},
	},
	{ 0, 2, 1, 1, 2, 3, }
};