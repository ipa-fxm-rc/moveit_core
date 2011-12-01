/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Ioan Sucan */

#ifndef COLLISION_DETECTION_FCL_COLLISION_ROBOT_
#define COLLISION_DETECTION_FCL_COLLISION_ROBOT_

#include "collision_detection/fcl/collision_common.h"
#include <fcl/broad_phase_collision.h>

namespace collision_detection
{

    class CollisionRobotFCL : public CollisionRobot
    {
    public:

        CollisionRobotFCL(const planning_models::KinematicModelPtr &kmodel, double padding = 0.0, double scale = 1.0);

        virtual void checkSelfCollision(const CollisionRequest &req, CollisionResult &res, const planning_models::KinematicState &state) const;

        virtual void checkSelfCollision(const CollisionRequest &req, CollisionResult &res, const planning_models::KinematicState &state, const AllowedCollisionMatrix &acm) const;

        virtual void checkOtherCollision(const CollisionRequest &req, CollisionResult &res, const planning_models::KinematicState &state,
                                         const CollisionRobot &other_robot, const planning_models::KinematicState &other_state) const;

        virtual void checkOtherCollision(const CollisionRequest &req, CollisionResult &res, const planning_models::KinematicState &state,
                                         const CollisionRobot &other_robot, const planning_models::KinematicState &other_state,
                                         const AllowedCollisionMatrix &acm) const;

    protected:

        virtual void updatedPaddingOrScaling(const std::vector<std::string> &links);

        fcl::CollisionObject* createCollisionObject(const shapes::Shape *shape) const;
        fcl::CollisionObject* createCollisionObject(const shapes::StaticShape *shape) const;
        fcl::CollisionObject* createCollisionObject(const shapes::Shape *shape, double scale, double padding) const;
        fcl::BroadPhaseCollisionManager* allocSelfCollisionBroadPhase(const planning_models::KinematicState &state) const;

        std::vector<planning_models::KinematicModel::LinkModel*> links_;
        std::vector<fcl::CollisionObject*>                       geoms_;
        std::map<std::string, CollisionObjectData*>              co_data_;
        std::map<std::string, std::size_t>                       index_map_;

        typedef std::map<boost::shared_ptr<planning_models::KinematicState::AttachedBodyProperties>, std::vector<fcl::CollisionObject*> > AttachedBodyObject;
        mutable AttachedBodyObject                               attached_bodies_;
        mutable boost::mutex::scoped_lock                        attached_bodies_lock_;
    };

}

#endif