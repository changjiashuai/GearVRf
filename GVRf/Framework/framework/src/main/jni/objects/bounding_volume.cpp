/* Copyright 2015 Samsung Electronics Co., LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/***************************************************************************
 * The bounding_volume for rendering.
 ***************************************************************************/

#include "bounding_volume.h"
#include "util/gvr_log.h"

namespace gvr {

BoundingVolume::BoundingVolume() {
    reset();
}

void BoundingVolume::reset() {
    center_ = glm::vec3(0.0f, 0.0f, 0.0f);
    radius_ = 0.0f;
    min_corner_ = glm::vec3(std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity());
    max_corner_ = glm::vec3(-std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity());
}

/* 
 * expand the current volume by the given point
 */
void BoundingVolume::expand(const glm::vec3 point) {
    if (min_corner_[0] > point[0]) {
        min_corner_[0] = point[0];
    }
    if (min_corner_[1] > point[1]) {
        min_corner_[1] = point[1];
    }
    if (min_corner_[2] > point[2]) {
        min_corner_[2] = point[2];
    }

    if (max_corner_[0] < point[0]) {
        max_corner_[0] = point[0];
    }
    if (max_corner_[1] < point[1]) {
        max_corner_[1] = point[1];
    }
    if (max_corner_[2] < point[2]) {
        max_corner_[2] = point[2];
    }

    updateCenterAndRadius();
}

void BoundingVolume::updateCenterAndRadius() {
    center_ = (min_corner_ + max_corner_) * 0.5f;
    if (min_corner_ == max_corner_) {
        radius_ = 0;
    } else {
        radius_ = glm::length(max_corner_ - min_corner_) * 0.5f;
    }
}

/*
 * expand the volume by the incoming center and radius
 */
void BoundingVolume::expand(const glm::vec3 &in_center, float in_radius) {
    glm::vec3 center_distance = in_center - center_;
    float length = glm::length(center_distance);

    //1. If the original volume is reset, simply use the incoming volume as the expanded one
    if (radius_ == 0) {
        radius_ = in_radius;
        center_ = in_center;
    }
    //2. If the two centers are the same and incoming radius is bigger, use the incoming radius with the same center
    else if (length == 0 && in_radius > radius_) {
        radius_ = in_radius;
    }
    // 3. If the incoming volume is not completely inside the original volume,
    // find the new center by taking the half-way point
    // between the two outer ends of the two spheres.
    // the radius is the distance between the two points
    // divided by two.
    else if ((length + in_radius) > radius_) {
        glm::normalize(center_distance);
        glm::vec3 c1 = in_center + (center_distance * in_radius);
        glm::vec3 c0 = center_ - (center_distance * radius_);
        center_ = (c0 + c1) * 0.5f;
        radius_ = glm::length(c1 - c0) * 0.5f;
    }

    // define the bounding box inside the sphere
    //       .. .. ..
    //     . -------/ .
    //    . |     r/ | .
    //    . |    /___| .
    //    . |      s | .
    //     .|________|.
    //       .. .. ..
    //
    // for a sphere:
    //           r^2 = s^2 + s^2 + s^2
    //           r^2 = (s^2)*3
    // sqrt((r^2)/3) = s
    //
    // r is radius_
    // s is side of the triangle
    //
    float side = (float) sqrt(((radius_ * radius_) / 3.0f));
    min_corner_ = glm::vec3(center_[0] - side, center_[1] - side,
            center_[2] - side);

    max_corner_ = glm::vec3(center_[0] + side, center_[1] + side,
            center_[2] + side);
}

/*
 * expand the volume by the incoming volume
 */
void BoundingVolume::expand(const BoundingVolume &volume) {
    expand(volume.min_corner());
    expand(volume.max_corner());
}

void BoundingVolume::transform(const BoundingVolume &in_volume,
        glm::mat4 matrix) {
    float a, b;

    //Inspired by Graphics Gems - TransBox.c
    //Transform the AABB to the correct position in world space
    //Generate a new AABB from the non axis aligned bounding box
    min_corner_.x = matrix[3].x;
    min_corner_.y = matrix[3].y;
    min_corner_.z = matrix[3].z;

    max_corner_.x = matrix[3].x;
    max_corner_.y = matrix[3].y;
    max_corner_.z = matrix[3].z;

    glm::vec3 in_min_corner = in_volume.min_corner();
    glm::vec3 in_max_corner = in_volume.max_corner();

    for (int i = 0; i < 3; i++) {
        //x coord
        a = matrix[i].x * in_min_corner.x;
        b = matrix[i].x * in_max_corner.x;
        if (a < b) {
            min_corner_.x += a;
            max_corner_.x += b;
        } else {
            min_corner_.x += b;
            max_corner_.x += a;
        }

        //y coord
        a = matrix[i].y * in_min_corner.y;
        b = matrix[i].y * in_max_corner.y;
        if (a < b) {
            min_corner_.y += a;
            max_corner_.y += b;
        } else {
            min_corner_.y += b;
            max_corner_.y += a;
        }

        //z coord
        a = matrix[i].z * in_min_corner.z;
        b = matrix[i].z * in_max_corner.z;
        if (a < b) {
            min_corner_.z += a;
            max_corner_.z += b;
        } else {
            min_corner_.z += b;
            max_corner_.z += a;
        }
    }

    updateCenterAndRadius();
}

bool BoundingVolume::intersect(glm::vec3& hitPoint, const glm::vec3& rayStart, const glm::vec3& rayDir)  const
{
	/* Algorithm from http://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms */
	glm::vec3 direction = glm::normalize(rayDir);
	glm::vec3 dirfrac;
	float t;

	dirfrac.x = 1.0f / direction.x;
	dirfrac.y = 1.0f / direction.y;
	dirfrac.z = 1.0f / direction.z;

	float t1 = (min_corner_.x - rayStart.x) * dirfrac.x;
	float t2 = (max_corner_.x - rayStart.x) * dirfrac.x;
	float t3 = (min_corner_.y - rayStart.y) * dirfrac.y;
	float t4 = (max_corner_.y - rayStart.y) * dirfrac.y;
	float t5 = (min_corner_.z - rayStart.z) * dirfrac.z;
	float t6 = (max_corner_.z - rayStart.z) * dirfrac.z;

	float tmin = glm::max( glm::max( glm::min(t1,t2), glm::min(t3,t4)), glm::min(t5,t6) );
	float tmax = glm::min( glm::min( glm::max(t1,t2), glm::max(t3,t4)), glm::max(t5,t6) );

     // Ray intersection, but whole AABB is behind us
	if (tmax < 0.0f)
	{
		t = tmax;
		return false;
	}
   // No intersection
	if(tmin > tmax)
	{
		t = tmax;
		return false;
	}
	t = tmin; // Store length of ray until intersection in t
	hitPoint = rayStart + direction * t; // intersection point
}

} // namespace

