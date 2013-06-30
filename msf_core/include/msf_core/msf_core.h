/*

Copyright (c) 2012, Simon Lynen, ASL, ETH Zurich, Switzerland
You can contact the author at <slynen at ethz dot ch>
Copyright (c) 2010, Stephan Weiss, ASL, ETH Zurich, Switzerland
You can contact the author at <stephan dot weiss at ieee dot org>

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
 * Neither the name of ETHZ-ASL nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ETHZ-ASL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#ifndef MSF_CORE_H_
#define MSF_CORE_H_

#include <vector>
#include <queue>

#include <Eigen/Eigen>

#include <msf_core/msf_sortedContainer.h>
#include <msf_core/msf_state.h>
#include <msf_core/msf_checkFuzzyTracking.h>

//good old days...
//#define N_STATE_BUFFER 256	///< size of unsigned char, do not change!

namespace msf_core{

enum{
  HLI_EKF_STATE_SIZE = 16 ///< number of states exchanged with external propagation. Here: p,v,q,bw,bw=16
};

template<typename EKFState_T>
class MSF_SensorManager;
template<typename EKFState_T>
class IMUHandler;

/** \class MSF_Core
 *
 * \brief The core class of the EKF
 * Does propagation of state and covariance
 * but also applying measurements and managing states and measurements
 * in lists sorted by time stamp
 */
template<typename EKFState_T>
class MSF_Core
{
  friend class MSF_MeasurementBase<EKFState_T>;
  friend class IMUHandler<EKFState_T>;
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW;

  enum{
    nErrorStatesAtCompileTime = EKFState_T::nErrorStatesAtCompileTime,  ///< error state length
    nStatesAtCompileTime = EKFState_T::nStatesAtCompileTime ///< complete state length
  };

  typedef typename EKFState_T::StateDefinition_T StateDefinition_T;
  typedef typename EKFState_T::StateSequence_T StateSequence_T;
  typedef Eigen::Matrix<double, nErrorStatesAtCompileTime, 1> ErrorState; ///< the error state type
  typedef Eigen::Matrix<double, nErrorStatesAtCompileTime, nErrorStatesAtCompileTime> ErrorStateCov; ///<the error state covariance type

  typedef msf_core::SortedContainer<EKFState_T> StateBuffer_T; ///< the type of the state buffer containing all the states
  typedef msf_core::SortedContainer<typename msf_core::MSF_MeasurementBase<EKFState_T>, typename msf_core::MSF_InvalidMeasurement<EKFState_T> > measurementBufferT; ///< the type of the measurement buffer containing all the measurements

  /**
   * \brief add a sensor measurement or an init measurement to the internal queue and apply it to the state
   * \param measurement the measurement to add to the internal measurement queue
   */
  void addMeasurement(shared_ptr<MSF_MeasurementBase<EKFState_T> > measurement);

  /**
   * \brief initializes the filter with the values of the given measurement, other init values from other
   * sensors can be passed in as "measurement" using the initMeasurement structs
   * \param measurement a measurement containing initial values for the state
   */
  void init(shared_ptr<MSF_MeasurementBase<EKFState_T> > measurement);

  /**
   * \brief finds the closest state to the requested time in the internal state
   * \param tstamp the time stamp to find the closest state to
   */
  shared_ptr<EKFState_T> getClosestState(double tstamp);

  /**
   * \brief returns the accumulated dynamic matrix between two states
   */
  void getAccumF_SC(const shared_ptr<EKFState_T>& state_old, const shared_ptr<EKFState_T>& state_new,
                    Eigen::Matrix<double, nErrorStatesAtCompileTime, nErrorStatesAtCompileTime>& F);
  /**
   * \brief returns previous measurement of the same type
   */
  shared_ptr<msf_core::MSF_MeasurementBase<EKFState_T> > getPreviousMeasurement(double time, int sensorID);

  /**
   * \brief finds the state at the requested time in the internal state
   * \param tstamp the time stamp to find the state to
   */
  shared_ptr<EKFState_T> getStateAtTime(double tstamp);

  /**
   * \brief propagates the error state covariance
   * \param state_old the state to propagate the covariance from
   * \param state_new the state to propagate the covariance to
   */
  void predictProcessCovariance(shared_ptr<EKFState_T>& state_old, shared_ptr<EKFState_T>& state_new);

  /**
   * \brief propagates the state with given dt
   * \param state_old the state to propagate from
   * \param state_new the state to propagate to
   */
  void propagateState(shared_ptr<EKFState_T>& state_old, shared_ptr<EKFState_T>& state_new);

  /**
   * \brief delete very old states and measurements from the buffers to free memory
   */
  void CleanUpBuffers();

  /**
   * \brief sets the covariance matrix of the core states to simulated values
   * \param P the error state covariance Matrix to fill
   */
  void setPCore(Eigen::Matrix<double, EKFState_T::nErrorStatesAtCompileTime, EKFState_T::nErrorStatesAtCompileTime>& P);

  /**
   * \brief ctor takes a pointer to an object which does the user defined calculations and provides interfaces for initialization etc.
   * \param usercalc the class providing the user defined calculations DO ABSOLUTELY NOT USE THIS REFERENCE INSIDE THIS CTOR!!
   */
  MSF_Core(const MSF_SensorManager<EKFState_T>& usercalc);
  ~MSF_Core();

  const MSF_SensorManager<EKFState_T>& usercalc() const;

private:


  /**
   * \brief get the index of the best state having no temporal drift at compile time
   */
  enum{
    indexOfStateWithoutTemporalDrift = msf_tmp::IndexOfBestNonTemporalDriftingState<StateSequence_T>::value
  };

  typedef typename msf_tmp::getEnumStateType<StateSequence_T, indexOfStateWithoutTemporalDrift>::value nonDriftingStateType; //returns void type for invalid types

  StateBuffer_T stateBuffer_; ///<EKF buffer containing pretty much all info needed at time t. sorted by t asc
  measurementBufferT MeasurementBuffer_; ///< EKF Measurements and init values sorted by t asc
  std::queue<shared_ptr<MSF_MeasurementBase<EKFState_T> > > queueFutureMeasurements_; ///< buffer for measurements to apply in future

  double time_P_propagated; ///< last time stamp where we have a valid propagation
  Eigen::Matrix<double, 3, 1> g_; ///< gravity vector

  bool initialized_; ///< is the filter initialized, so that we can propagate the state?
  bool predictionMade_; ///< is there a state prediction, so we can apply measurements?


  bool isfuzzyState_; ///< was the filter pushed to fuzzy state by a measurement?

  CheckFuzzyTracking<EKFState_T, nonDriftingStateType> fuzzyTracker_;  ///< watch dog to determine fuzzy tracking by observing non temporal drifting states
  const MSF_SensorManager<EKFState_T>& usercalc_; ///< a class which provides methods for customization of several calculations

  /**
   * \brief applies the correction
   * \param delaystate the state to apply the correction on
   * \param correction the correction vector
   * \param fuzzythres the error of the non temporal drifting state allowed before fuzzy tracking will be triggered
   */
  bool applyCorrection(shared_ptr<EKFState_T>& delaystate, ErrorState & correction, double fuzzythres = 0.1);

  /**
   * \brief propagate covariance to a given state in time
   * \param state the state to propagate to from the last propagated time
   */
  void propPToState(shared_ptr<EKFState_T>& state);

  //internal state propagation
  /**
   * \brief This function gets called on incoming imu messages
   * and then performs the state prediction internally.
   * Only use this OR stateCallback by remapping the topics accordingly.
   * \param msg the imu ros message
   * \sa{stateCallback}
   */
  void process_imu(const msf_core::Vector3&linear_acceleration,
                   const msf_core::Vector3&angular_velocity, const double& msg_stamp, size_t msg_seq);

  /// external state propagation
  /**
   * \brief This function gets called when state prediction is performed externally,
   * e.g. by asctec_mav_framework. Msg has to be the latest predicted state.
   * Only use this OR imuCallback by remapping the topics accordingly.
   * \param msg the state message from the external propagation
   * \sa{imuCallback}
   */
  void process_extstate(const msf_core::Vector3& linear_acceleration,
                     const msf_core::Vector3& angular_velocity, const msf_core::Vector3& p,
                     const msf_core::Vector3& v, const msf_core::Quaternion& q, bool is_already_propagated, const double& msg_stamp,
                     size_t msg_seq);

  /// propagates P by one step to distribute processing load
  void propagatePOneStep();

  ///checks the queue of measurements to be applied in the future
  void handlePendingMeasurements();

};

};// end namespace

#include <msf_core/implementation/msf_core.hpp>

#endif /* MSF_CORE_H_ */
