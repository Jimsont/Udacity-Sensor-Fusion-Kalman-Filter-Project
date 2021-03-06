#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using Eigen::MatrixXd;
using Eigen::VectorXd;
using namespace std;
/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 3.0;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 1.0;
  
  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  
  /**
   * End DO NOT MODIFY section for measurement noise values 
   */
  
  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */
  // initially set to false, set to true in first call of ProcessMeasurement
  is_initialized_ = false;

  // Init State dimension
  n_x_ = 5;

  // Init Augmented state dimension
  n_aug_ = 7;

  // Init Sigma point spreading parameter
  lambda_ = 3-n_aug_;

  // init NIS_las;
  NIS_las = 0.0;

  //init NIS_radr;
  NIS_radr = 0.0;
  
  // Init Weights of sigma points
  /*
  weights_ = VectorXd(2);
  weights_(0) = lambda_/(lambda_+n_aug_);
  weights_(1) = 0.5/(lambda_+n_aug_);
  */
  weights_ = VectorXd(1+2*n_aug_);
  double weight0 = lambda_/(lambda_+n_aug_);
  double weight = 0.5/(lambda_+n_aug_);
  weights_(0) = weight0;
  for (int i = 1; i<1+2*n_aug_; i++)
  {
    weights_(i) = weight;
  }

  // init state vector: 
  // [pos1 pos2 vel_abs yaw_angle yaw_rate] in SI units and rad
  // set to zero 
  x_ = VectorXd::Zero(n_x_);

  // init state covariance matrix
  /*
  VectorXd ve = VectorXd(n_x_);
  ve(0) = 0.5*0.034*0.034*std_a_;
  ve(1) = 0.5*0.034*0.034*std_a_;
  ve(2) = 0.034*std_a_;
  ve(3) = 0.5*0.034*0.034*std_yawdd_;
  ve(4) = 0.034*std_yawdd_;
  P_ = ve*ve.transpose();
  */
  P_ = MatrixXd::Identity(n_x_, n_x_);
  P_(0,0) = 0.01;
  P_(1,1) = 0.01;
  P_(2,2) = 0.5;
  P_(3,3) = 0.5;
  P_(4,4) = 0.5;

  // init laser measurement noise covariance matrix
  R_las = MatrixXd::Zero(2, 2);
  R_las(0,0) = std_laspx_*std_laspx_;
  R_las(1,1) = std_laspy_*std_laspy_;

  // init radar measurement noise covariance matrix
  R_radr = MatrixXd::Zero(3, 3);
  // set value for radar noise cocariance matrix
  R_radr(0,0) = std_radr_*std_radr_;
  R_radr(1,1) = std_radphi_*std_radphi_;
  R_radr(2,2) = std_radrd_*std_radrd_;

  // predicted sigma points matrix
  Xsig_pred_ = MatrixXd::Zero(n_x_, 1+2*n_aug_);

  // time when the state is true, in us
  time_us_ = 0.0;
}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */

  // if is_initialized_ = false,
  // it is the first time calling ProceeMeasurement.
  // For first time calling, 
  // treat current measurement as updated ukf update results
  // If not, run predictio. Then, run update function according to sensor type
  if (is_initialized_ == false)
  {
    // init imestamp
    time_us_ = meas_package.timestamp_;

    if (meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_ == true)
    {
      x_(0) = meas_package.raw_measurements_(0);
      x_(1) = meas_package.raw_measurements_(1);
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_ == true)
    {
      double rho = meas_package.raw_measurements_(0);
      double phi = meas_package.raw_measurements_(1);
      double rho_dot = meas_package.raw_measurements_(2);
      x_(0) = rho*cos(phi);
      x_(1) = rho*sin(phi);
    }
    // else
    // {
    //   std::cout<<"invalid sensor type"<<'\n';
    // }
    is_initialized_ = true;
    // return;
    // std::cout<<"run init"<<'\n';
  }
  else
  {
    double dt = (meas_package.timestamp_-time_us_)/1000000.0;
    time_us_ = meas_package.timestamp_;
    // cout<<dt<<'\n';
    Prediction(dt);
    // std::cout<<"run prediction"<<'\n';
    if (meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_ == true)
    {
      // std::cout<<"run lidar update start"<<'\n';
      UpdateLidar(meas_package);
      
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_ == true)
    {
      // std::cout<<"run radar update start"<<'\n';
      UpdateRadar(meas_package);
    }
    else
    {
      std::cout<<"invalid sensor type"<<'\n';
    }
  }
}

void UKF::Prediction(double delta_t) {
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */

  /*
  create augment sigma points
  predict augment sigma points
  Then, update Xsig_pred_ matrix
  Then, predict state covariance matrix
  */

  // create augment sigma points
  MatrixXd Xsig_aug = MatrixXd::Zero(n_aug_, 2*n_aug_+1);
  MatrixXd P_aug = MatrixXd::Zero(n_aug_, n_aug_);
  P_aug.topLeftCorner(n_x_, n_x_) = P_;
  P_aug(5, 5) = std_a_*std_a_;
  P_aug(6, 6) = std_yawdd_*std_yawdd_;
  MatrixXd A = P_aug.llt().matrixL();
  VectorXd x_aug = VectorXd::Zero(n_aug_);
  x_aug.head(n_x_) = x_;

  Xsig_aug.col(0) = x_aug;
  for (int i = 0; i<n_aug_; i++)
  {
    Xsig_aug.col(i+1) = x_aug + sqrt(lambda_ + n_aug_) * A.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * A.col(i);
  }

  // predict sigma point
  for (int i=0; i<2*n_aug_+1; i++)
  {
    double px = Xsig_aug(0,i);
    double py = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawd = Xsig_aug(4,i);
    double nu_a = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);

    // predict state
    double px_p, py_p;

    // when abs(yawd) is close to zero, switch to model for yawd = 0
    if (fabs(yawd)>0.001)
    {
      px_p = px + v/yawd * (sin(yaw+yawd*delta_t)-sin(yaw));
      py_p = py + v/yawd * (cos(yaw)- cos(yaw+yawd*delta_t));
    }
    else
    {
      px_p = px + v*delta_t*cos(yaw);
      py_p = py + v*delta_t*sin(yaw);
    }
    
    double v_p = v;
    double yaw_p = yaw+yawd*delta_t;
    double yawd_p = yawd;

    // add noise
    px_p = px_p + 0.5*nu_a*delta_t*delta_t*cos(yaw);
    py_p = py_p + 0.5*nu_a*delta_t*delta_t*sin(yaw);
    v_p = v_p + nu_a*delta_t;
    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;

    // update Xsig_pred
    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
  }

  // calculate mean prediction state and update x_ state vector
  VectorXd x_mean = VectorXd::Zero(n_x_);
  for (int i=0; i<2*n_aug_+1; i++)
  {
    x_mean = x_mean + weights_(i)*Xsig_pred_.col(i);
  }
  // update x_ state vector by mean prediction
  x_ = x_mean;

  // predict state covariance matrix
  MatrixXd P_p = MatrixXd::Zero(n_x_, n_x_);
  for (int i=0; i<2*n_aug_+1; i++)
  {
    VectorXd x_diff = Xsig_pred_.col(i)-x_mean;
    // angle normalization
    while(x_diff(3)>M_PI) x_diff(3) = x_diff(3)-2.0*M_PI;
    while(x_diff(3)<-M_PI) x_diff(3) = x_diff(3)+2.0*M_PI;

    P_p = P_p + weights_(i)*x_diff*x_diff.transpose();
  }

  // update state covariance matrix
  P_ = P_p;
}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use lidar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */
  VectorXd z_real = meas_package.raw_measurements_;

  // dimension of measurement space
  int n_z = z_real.size();
  // std::cout<<"measurement dimension"<<'\n';

  // map sigma points of state space to sigma points of measurement space
  MatrixXd Zsig_pred = Xsig_pred_.topRows(n_z);

  // calculate predicted measurement mean
  // lidar measurement space is the same as state space, so directly copy first px, py mean state estimation
  VectorXd z_pred = VectorXd::Zero(n_z);
  z_pred(0) = x_(0);
  z_pred(1) = x_(1);
  /*
  for (int i=0; i<2*n_aug_+1; i++)
  {
    z_pred = z_pred + weights_(i)*Zsig_pred.col(i);
  }
  */

  // calculate predicted measurement covariance matrix
  MatrixXd S = MatrixXd::Zero(n_z,n_z);
  for (int i=0; i<2*n_aug_+1; i++)
  {
    VectorXd z_diff = Zsig_pred.col(i)-z_pred;
    S = S + weights_(i)*z_diff*z_diff.transpose();
  }
  S = S+R_las;

  // calculate cross-correlation matrix
  MatrixXd T = MatrixXd::Zero(n_x_, n_z);
  for (int i=0; i<2*n_aug_+1; i++)
  {
    VectorXd z_diff = Zsig_pred.col(i)-z_pred;
    VectorXd x_diff = Xsig_pred_.col(i)-x_;
    while(x_diff(3)>M_PI) x_diff(3) = x_diff(3)-2.0*M_PI;
    while(x_diff(3)<-M_PI) x_diff(3) = x_diff(3)+2.0*M_PI;

    T = T + weights_(i)*x_diff*z_diff.transpose();
  }

  // calculate Kalman filter
  MatrixXd K = T*S.inverse();

  // update state prediction and covarance matrix
  VectorXd z_diff = (z_real-z_pred);
  x_ = x_ + K*z_diff;
  P_ = P_ - K*S*K.transpose();

  // NIS laser
  NIS_las = z_diff.transpose()*S.inverse()*z_diff; 
}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use radar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */
  VectorXd z_real = meas_package.raw_measurements_;

  // dimension of measurement space
  int n_z = z_real.size();
  // std::cout<<"measurement dimension"<<'\n';

  // map sigma points of state space to sigma points of measurement space
  MatrixXd Zsig_pred = MatrixXd::Zero(n_z, 2*n_aug_+1);
  for (int i=0; i<2*n_aug_+1; i++)
  {
    double px = Xsig_pred_(0,i);
    double py = Xsig_pred_(1,i);
    double v = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);
    Zsig_pred(0,i) = sqrt(px*px+py*py);
    Zsig_pred(1,i) = atan2(py,px);
    // Zsig_pred(2,i) = (px*cos(yaw)*v + py*sin(yaw)*v)/sqrt(px*px+py*py);
    // avoid zero division
    if (sqrt(px*px+py*py)<=0.00001)
    {
      Zsig_pred(2,i) = (px*cos(yaw)*v + py*sin(yaw)*v)/0.00001;
    }
    else
    {
      Zsig_pred(2,i) = (px*cos(yaw)*v + py*sin(yaw)*v)/sqrt(px*px+py*py);
    } 
  }
  // std::cout<<"map sigma points"<<'\n';

  // calculate predicted measurement mean
  VectorXd z_pred = VectorXd::Zero(n_z);
  for (int i=0; i<2*n_aug_+1; i++)
  {
    z_pred = z_pred + weights_(i)*Zsig_pred.col(i);
  }
  // std::cout<<"z_pred size = "<<z_pred.size()<<'\n';

  // calculate predicted measurement covariance matrix
  MatrixXd S = MatrixXd::Zero(n_z,n_z);
  for (int i=0; i<2*n_aug_+1; i++)
  {
    VectorXd z_diff = Zsig_pred.col(i)-z_pred;
    while(z_diff(1)>M_PI) z_diff(1) = z_diff(1)-2.0*M_PI;
    while(z_diff(1)<-M_PI) z_diff(1) = z_diff(1)+2.0*M_PI;
    S = S + weights_(i)*z_diff*z_diff.transpose();
  }
  S = S+R_radr;
  // std::cout<<"S dimension= "<<S.rows()<<"x"<<S.cols()<<'\n';


  // calculate cross-correlation matrix
  MatrixXd T = MatrixXd::Zero(n_x_, n_z);
  for (int i=0; i<2*n_aug_+1; i++)
  {
    VectorXd z_diff = Zsig_pred.col(i)-z_pred;
    while(z_diff(1)>M_PI) z_diff(1) = z_diff(1)-2.0*M_PI;
    while(z_diff(1)<-M_PI) z_diff(1) = z_diff(1)+2.0*M_PI;

    VectorXd x_diff = Xsig_pred_.col(i)-x_;
    while(x_diff(3)>M_PI) x_diff(3) = x_diff(3)-2.0*M_PI;
    while(x_diff(3)<-M_PI) x_diff(3) = x_diff(3)+2.0*M_PI;

    T = T + weights_(i)*x_diff*z_diff.transpose();
  }

  // calculate Kalman filter
  MatrixXd K = T*S.inverse();

  // update state prediction and covarance matrix
  VectorXd z_diff = (z_real-z_pred);
  while(z_diff(1)>M_PI) z_diff(1) = z_diff(1)-2.0*M_PI;
  while(z_diff(1)<-M_PI) z_diff(1) = z_diff(1)+2.0*M_PI;
  x_ = x_ + K*z_diff;
  P_ = P_ - K*S*K.transpose();

  // NIS laser
  NIS_radr = z_diff.transpose()*S.inverse()*z_diff; 

}