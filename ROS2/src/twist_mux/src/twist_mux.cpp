// Copyright 2020 PAL Robotics S.L.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the PAL Robotics S.L. nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/*
 * @author Enrique Fernandez
 * @author Siegfried Gevatter
 * @author Jeremie Deray
 */

#include <twist_mux/twist_mux.hpp>
#include <twist_mux/topic_handle.hpp>
#include <twist_mux/twist_mux_diagnostics.hpp>
#include <twist_mux/twist_mux_diagnostics_status.hpp>
#include <twist_mux/utils.hpp>
#include <twist_mux/params_helpers.hpp>

#include <list>
#include <memory>
#include <string>

/**
 * @brief hasIncreasedAbsVelocity Check if the absolute velocity has increased
 * in any of the components: linear (abs(x)) or angular (abs(yaw))
 * @param old_twist Old velocity
 * @param new_twist New velocity
 * @return true is any of the absolute velocity components has increased
 */
bool hasIncreasedAbsVelocity(
  const geometry_msgs::msg::Twist & old_twist,
  const geometry_msgs::msg::Twist & new_twist)
{
  const auto old_linear_x = std::abs(old_twist.linear.x);
  const auto new_linear_x = std::abs(new_twist.linear.x);

  const auto old_angular_z = std::abs(old_twist.angular.z);
  const auto new_angular_z = std::abs(new_twist.angular.z);

  return (old_linear_x < new_linear_x) || (old_angular_z < new_angular_z);
}

namespace twist_mux
{
// see e.g. https://stackoverflow.com/a/40691657
constexpr std::chrono::duration<int64_t> TwistMux::DIAGNOSTICS_PERIOD;

TwistMux::TwistMux()
: Node("twist_mux", "",
    rclcpp::NodeOptions().allow_undeclared_parameters(
      true).automatically_declare_parameters_from_overrides(true))
{
}

/*
* use_stamped kontrolise da li ce twist_mux koristiti:
* 1) geometry_msg::msg::Twist
*    linear {x, y, z}
*    angular {x, y, z}
* 2) geometru_msg::msg::TwistStamped
*    header:
*      stamp: vrijeme kad je poruka vazila
*      frame_id: koordinatni sistem
*    twist:
*      linear {x, y, z}
*      angular {x, y, z}
*/
void TwistMux::init()
{
  // Get use stamped parameter
  bool use_stamped = true;
  this->declare_parameter("use_stamped", use_stamped); // Registurje parametar pod imenom "use_stamped" u ROS2 sistemu.

  auto nh = std::shared_ptr<rclcpp::Node>(this, [](rclcpp::Node *) {});
  fetch_param(nh, "use_stamped", use_stamped); // Pomocna wrap metoda za pronalazanje parametara

  /// Get topics and locks:
  if(use_stamped)
  {
    velocity_stamped_hs_ = std::make_shared<velocity_stamped_topic_container>();
    getTopicHandles("topics", *velocity_stamped_hs_);
  }
  else
  {
    velocity_hs_ = std::make_shared<velocity_topic_container>();
    getTopicHandles("topics", *velocity_hs_);
  }
  lock_hs_ = std::make_shared<lock_topic_container>();
  getTopicHandles("locks", *lock_hs_);

  /// Publisher for output topic:
  if(use_stamped)
  {
    cmd_pub_stamped_ =
      this->create_publisher<geometry_msgs::msg::TwistStamped>(
        "cmd_vel_out",
        rclcpp::QoS(rclcpp::KeepLast(1)));
  }
  else
  {
    cmd_pub_ =
      this->create_publisher<geometry_msgs::msg::Twist>(
      "cmd_vel_out",
      rclcpp::QoS(rclcpp::KeepLast(1)));
  }

  /// Diagnostics:
  // Kreiramo objekat klase TwistMuxDiagnostics koji je vezan na ovaj node
  // On će pratiti status i slati podatke u ROS2 diagnostics sistem (/diagnostics topic)
  diagnostics_ = std::make_shared<diagnostics_type>(this);

  // status_ je struktura u kojoj se čuvaju pokazivači na sve izvore i lockove
  // Popunjavamo koje liste treba pratiti
  status_ = std::make_shared<status_type>();
  status_->velocity_hs = velocity_hs_;
  status_->velocity_stamped_hs = velocity_stamped_hs_;
  status_->lock_hs = lock_hs_;
  status_->use_stamped = use_stamped;
  
  // Tajmer koji se pokreće svakih DIAGNOSTICS_PERIOD sekundi (~1 sekund)
  // Svaki put kad otkuca, poziva se metoda updateDiagnostics()
  // Ta metoda ažurira status i prosleđuje ga TwistMuxDiagnostics objektu
  // Ova linija koda postavlja periodičnu proveru zdravlja sistema.
  diagnostics_timer_ = this->create_wall_timer(
    DIAGNOSTICS_PERIOD, [this]() -> void {
      updateDiagnostics();
    });
}

void TwistMux::updateDiagnostics()
{
  // status_->priority je vrednost koju dijagnostika kasnije koristi da odredi: koji izvori brzine su trenutno maskirani, ko ima prednost u odnosu na druge, i koje lock-ove treba prikazati kao aktivne.
  status_->priority = getLockPriority();
  RCLCPP_DEBUG(get_logger(), "updateDiagnostics: lol");
  diagnostics_->updateStatus(status_);
  RCLCPP_DEBUG(get_logger(), "returned from updateStatus");
}

void TwistMux::publishTwist(const geometry_msgs::msg::Twist::ConstSharedPtr & msg)
{
  // cmd_pub_ je publisher za izlazni topic cmd_vel_out
  cmd_pub_->publish(*msg);
}

void TwistMux::publishTwistStamped(const geometry_msgs::msg::TwistStamped::ConstSharedPtr & msg)
{
  // cmd_pub_stamped_ je publisher za izlazni topic cmd_vel_out
  cmd_pub_stamped_->publish(*msg);
}

/**
 * @brief Učita konfiguraciju topika iz parametara (YAML fajla) i napravi "handle" objekte.
 *
 * @tparam T – tip handle-a (VelocityTopicHandle, VelocityStampedTopicHandle ili LockTopicHandle).
 * @param param_name – prefiks u parametru (npr. "topics" ili "locks").
 * @param topic_hs – lista u koju se dodaju kreirani handle-ovi.
 *
 * Funkcija radi sledeće:
 * 1. Pretraži sve parametre u okviru `param_name` (npr. "topics.joystick", "topics.navigation"...).
 * 2. Za svaki pronađeni prefiks pročita njegovu vrijednostS
 * 3. Na osnovu ovih parametara kreira odgovarajući TopicHandle i dodaje ga u listu.
 *
 * Na kraju, "twist_mux" ima listu svih izvora brzinskih komandi ili lock-ova koje može da prati i upoređuje po prioritetu.
 */
template<typename T>
void TwistMux::getTopicHandles(const std::string & param_name, std::list<T> & topic_hs)
{
  RCLCPP_DEBUG(get_logger(), "getTopicHandles: %s", param_name.c_str());

  rcl_interfaces::msg::ListParametersResult list = list_parameters({param_name}, 10);

  try {
    for (auto prefix : list.prefixes) {
      RCLCPP_DEBUG(get_logger(), "Prefix: %s", prefix.c_str());

      std::string topic;
      double timeout = 0;
      int priority = 0;
      bool expire_on_idle = false;

      auto nh = std::shared_ptr<rclcpp::Node>(this, [](rclcpp::Node *) {});

      fetch_param(nh, prefix + ".topic", topic);
      fetch_param(nh, prefix + ".timeout", timeout);
      fetch_param(nh, prefix + ".priority", priority);
      fetch_param(nh, prefix + ".expire_on_idle", expire_on_idle);

      RCLCPP_DEBUG(get_logger(), "Retrieved topic: %s", topic.c_str());
      RCLCPP_DEBUG(get_logger(), "Listed timeout: %.2f", timeout);
      RCLCPP_DEBUG(get_logger(), "Listed priority: %d", priority);
      RCLCPP_DEBUG(get_logger(), "Listed expire_on_idle: %s", expire_on_idle ? "true" : "false");

      topic_hs.emplace_back(prefix, topic, std::chrono::duration<double>(timeout), priority, expire_on_idle, this);
    }
  } catch (const ParamsHelperException & e) {
    RCLCPP_FATAL(get_logger(), "Error parsing params '%s':\n\t%s", param_name.c_str(), e.what());
    throw e;
  }
}

// Vraća najveći prioritet među aktivnim lockovima (ako nema lockova → 0).
// Taj broj služi kao granica: svi izvori sa nižim ili jednakim prioritetom od aktivnog locka → smatraju se blokiranim.
int TwistMux::getLockPriority()
{
  LockTopicHandle::priority_type priority = 0;

  /// max_element on the priority of lock topic handles satisfying
  /// that is locked:
  for (const auto & lock_h : *lock_hs_) {
    if (lock_h.isLocked()) {
      auto tmp = lock_h.getPriority();
      if (priority < tmp) {
        priority = tmp;
      }
    }
  }

  RCLCPP_DEBUG(get_logger(), "Priority = %d.", static_cast<int>(priority));

  return priority;
}

// Vraca odgovor da li trenutni izvor (twist) ima najveci prioritet i nije blokiran lock-om.
bool TwistMux::hasPriority(const VelocityTopicHandle & twist)
{
  // Uzimamo najveći prioritet aktivnog lock-a
  const auto lock_priority = getLockPriority();

  LockTopicHandle::priority_type priority = 0;
  std::string velocity_name = "NULL";

  /// max_element on the priority of velocity topic handles satisfying
  /// that is NOT masked by the lock priority:
  // Prolazimo kroz sve izvore brzinskih komandi (joystick, nav2, keyboard...)
  for (const auto & velocity_h : *velocity_hs_) {
    // Ako izvor NIJE maskiran (nije istekao i nije ispod lock prioriteta)
    if (!velocity_h.isMasked(lock_priority)) {
      const auto velocity_priority = velocity_h.getPriority();
      // Čuvamo onaj sa najvećim prioritetom
      if (priority < velocity_priority) {
        priority = velocity_priority;
        velocity_name = velocity_h.getName();
      }
    }
  }

  // Vraćamo true ako je ovaj konkretni izvor (twist) baš onaj koji trenutno ima najveći prioritet
  return twist.getName() == velocity_name;
}


bool TwistMux::hasPriorityStamped(const VelocityStampedTopicHandle & twist)
{
  const auto lock_priority = getLockPriority();

  LockTopicHandle::priority_type priority = 0;
  std::string velocity_name = "NULL";

  /// max_element on the priority of velocity topic handles satisfying
  /// that is NOT masked by the lock priority:
  for (const auto & velocity_stamped_h : *velocity_stamped_hs_) {
    if (!velocity_stamped_h.isMasked(lock_priority)) {
      const auto velocity_priority = velocity_stamped_h.getPriority();
      if (priority < velocity_priority) {
        priority = velocity_priority;
        velocity_name = velocity_stamped_h.getName();
      }
    }
  }

  return twist.getName() == velocity_name;
}

}  // namespace twist_mux
