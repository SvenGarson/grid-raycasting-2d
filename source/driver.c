#include "sdl2_setup.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL_opengl.h>
#include "datatypes.h"
#include "helpers.h"
#include <math.h>

/*
    Keeping track of things
    -----------------------

    Things to improve & think about:
      - Test how the max buffer behaves over long distances
      - Are there any duplicate entries in the generated list?
      - Avoid sorting like hell, this will cripple long raycasts - rather
        optimize the algorithm and sort impact points right away by considering
        both axis for every step and let an axis that is out of max steps be
        ignored for future runs, until the second one runs out too
*/

// Constants
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Function declarations
//   Raycasting related function declarations
void generateRaycastPointsAlongEdges(void);

//   Housekeeping related function declarations
void initializeState(void);
void gameloop(SSDL2SetupResult setup_result);
void poll_and_consume_input(bool * request_to_exit_application);
void update_scene(void);
void render_scene(SSDL2SetupResult setup_result);

// Compilation unit private state
//   Input related state
SVec2i mouse_screen_position = { 0, 0 };
bool mouse_left_button_held = false;
bool mouse_right_button_held = false;

//   Grid related state
const SVec2i gridTileDimensions = { 20, 20 };
const SVec2i tilesOnGridAxis = { SCREEN_WIDTH / gridTileDimensions.x, SCREEN_HEIGHT / gridTileDimensions.y };
const SVec2i gridDimensions = { tilesOnGridAxis.x * gridTileDimensions.x, tilesOnGridAxis.y * gridTileDimensions.y };
const SVec2i gridOriginBottomLeft = { 0, 0 };
const SAABB4f grid_bounding_box = {
  { gridOriginBottomLeft.x, gridOriginBottomLeft.y },
  { gridOriginBottomLeft.x + gridDimensions.x, gridOriginBottomLeft.y + gridDimensions.y },
};

// Raycasting related state
SVec2f raycast_origin = { 0, 0 };
SVec2f raycast_vector = { 0, 0 };
SVec2f raycast_destination = { 0, 0 };

// Raycast impact point generation related state

// Main function
int main(__attribute__((unused))int argc, __attribute__((unused))char * arvg[])
{
  SSDL2SetupResult result = sdl2_setup_for_2d_rendering(SCREEN_WIDTH, SCREEN_HEIGHT, "2dTileRaycasting ");
  if (result.setup_result != SDL2_SETUP_SUCCESS)
  {
    // SDL2 is not usable at this point
    printf("\nSDL2 could not be setup. Exiting ...");
    return -1;
  }

  // Initialize before invoking the game loop
  initializeState();

  // Gameloop - When done consider the game closing
  gameloop(result);

  // Free used resources
  sdl2_setup_teardown(result);

  // Back to OS
  return 0;
}

void initializeState() {
  // Flush output ...
  printf("\n");
  fflush(stdout);
}

// Function definitions
void update_scene(void) {
  // Control raycasting origin and vector
  if (mouse_right_button_held == true)
  {
    // Update origin position
    raycast_origin = (SVec2f) { (float)mouse_screen_position.x, (float)mouse_screen_position.y };
    // Update the vector
    raycast_vector = (SVec2f) {
      raycast_destination.x - raycast_origin.x,
      raycast_destination.y - raycast_origin.y
    };
  }

  if (mouse_left_button_held == true)
  {
    // Track the end of the raycast
    raycast_destination = (SVec2f) {mouse_screen_position.x, mouse_screen_position.y};
    // Determine the vector
    raycast_vector = (SVec2f) {
      (float)mouse_screen_position.x - raycast_origin.x,
      (float)mouse_screen_position.y - raycast_origin.y,
    };
  }

  // Generate points up to length for horizontal and vertical edges
  generateRaycastPointsAlongEdges();
  /*

          - generate hori and verti edge points upto ray length
          > for every impact save entry with the following data points:
            - impact point vec2f
            - delta time from origin to impact for ray length
            - ray length and/or the ray vector
            - distance from ray origin to impact (used for sorting etc. later)

      # Sort and order generated points by delta time

          - sort both lists into a single list

      # Detect all tiles the raycast hits based on computed points

          - make list of tiles; impact points etc. (whatever is needed)

      # Test against edge cases

        - perfectly vertical and horizontal
        - zero length
        - huge length
        - dense grid
        - cell width and height differ
  */
}

int sort_impact_information(const void * info_left, const void * info_right)
{
    const SImpactInformation * p_info_left = (const SImpactInformation *)info_left;
    const SImpactInformation * p_info_right = (const SImpactInformation *)info_right;

    if (p_info_left->impact_time < p_info_right->impact_time)
      return -1;
    else if (p_info_left->impact_time > p_info_right->impact_time)
      return 1;
    else
      return 0;
}

void generateRaycastPointsAlongEdges(void) {
    // Parameterize list building
    const int MAX_POINTS_PER_AXIS = 256;
    int points_recorded_raycast_x = 0;
    int points_recorded_raycast_y = 0;
    SImpactInformation * const p_info_raycast_x = malloc(sizeof(SImpactInformation) * MAX_POINTS_PER_AXIS);
    SImpactInformation * const p_info_raycast_y = malloc(sizeof(SImpactInformation) * MAX_POINTS_PER_AXIS);

    // Start computing all impact points
    const SVec2f raycast_grid_relative_origin = {
      (raycast_origin.x - gridOriginBottomLeft.x),
      (raycast_origin.y - gridOriginBottomLeft.y)
    };

    // Consider raycast only when ray origin inside the grid
    const bool ray_origin_out_of_grid_bounds =
      (raycast_grid_relative_origin.x < 0.0f)              ||
      (raycast_grid_relative_origin.x >= gridDimensions.x) ||
      (raycast_grid_relative_origin.y < 0.0f)              ||
      (raycast_grid_relative_origin.y >= gridDimensions.y);

    if (ray_origin_out_of_grid_bounds) return;

    // Determine first impact point between the ray and the tile it is contained in based
    // on the ray direction and the first edge the ray impacts for each axis
    const SVec2i raycast_tile_origin = {
      (int)(raycast_grid_relative_origin.x / gridTileDimensions.x),
      (int)(raycast_grid_relative_origin.y / gridTileDimensions.y)
    };
    const SVec2i raycast_direction = helper_vector_direction(raycast_vector);

    // When the ray is positive (x or y axis) it must be swept against the right tile
    // edge, if it is negative (x or y axis) it must be swept against the left tile edge
    const SVec2i tile_edge_in_ray_direction = {
      raycast_direction.x > 0 ? (raycast_tile_origin.x + raycast_direction.x) * gridTileDimensions.x : raycast_tile_origin.x * gridTileDimensions.x,
      raycast_direction.y > 0 ? (raycast_tile_origin.y + raycast_direction.y) * gridTileDimensions.y : raycast_tile_origin.y * gridTileDimensions.y,
    };

    // Determine impact time between the ray vector and the tile containing the origin
    // Beware of ray vector axis parallel to any grid axis
    const SVec2f raycast_grid_origin = { raycast_origin.x - gridOriginBottomLeft.x, raycast_origin.y - gridOriginBottomLeft.y};
    const SVec2f intersect_times_initial = {
      (tile_edge_in_ray_direction.x - raycast_grid_origin.x) / raycast_vector.x,
      (tile_edge_in_ray_direction.y - raycast_grid_origin.y) / raycast_vector.y
    };

    // Determine if the ray hits the edge and if, at what exact position
    const SVec2b initial_ray_hits = {
      raycast_direction.x != 0 && intersect_times_initial.x < 1.0f,
      raycast_direction.y != 0 && intersect_times_initial.y < 1.0f,
    };

    // Pre-compute data needed for both axis
    const float raycast_length = sqrtf(raycast_vector.x * raycast_vector.x + raycast_vector.y * raycast_vector.y);

    // Step through all impact points for vertical tile edges
    if (raycast_direction.x != 0 && initial_ray_hits.x == true)
    {
      // Note: We know the x axis of the ray hit the vertical tile edge containing the ray origin
      SVec2f initial_impact_position_raycast_x = {
        raycast_origin.x + raycast_vector.x * intersect_times_initial.x,
        raycast_origin.y + raycast_vector.y * intersect_times_initial.x
      };

      bool initial_impact_position_raycast_x_oob =
        initial_impact_position_raycast_x.x < grid_bounding_box.min.x ||
        initial_impact_position_raycast_x.x > grid_bounding_box.max.x ||
        initial_impact_position_raycast_x.y < grid_bounding_box.min.y ||
        initial_impact_position_raycast_x.y > grid_bounding_box.max.y;

      if (initial_impact_position_raycast_x_oob) return;

      // Determine which tile the impact hits
      SVec2i tiled_impact_point = {
        tile_edge_in_ray_direction.x / gridTileDimensions.x,
        initial_impact_position_raycast_x.y / gridTileDimensions.y
      };
      SVec2i impacted_tile_based_on_ray = {
        raycast_vector.x >= 0 ? tiled_impact_point.x : tiled_impact_point.x - 1,
        tiled_impact_point.y
      };

      // Record initial impact on vertical edge
      if (points_recorded_raycast_x < MAX_POINTS_PER_AXIS) {
        p_info_raycast_x[points_recorded_raycast_x++] = (SImpactInformation) {
          intersect_times_initial.x,
          initial_impact_position_raycast_x,
          impacted_tile_based_on_ray
        };
      }

      // Note: We know the vertical edge hit is in bound of the grid
      // Now determine all subsequent impacts on the vertical edges
      //
      // Do not accumulate the distances but rather determine the required lengths to avoid rounding errors
      // over large raycast distances
      int step_index = 1;
      const float horizontal_edge_stepsize = fabs((raycast_vector.y * gridTileDimensions.x) / raycast_vector.x);
      const SVec2f step_size = { gridTileDimensions.x * raycast_direction.x, horizontal_edge_stepsize * raycast_direction.y };

      // Account for the distance from origin to the initial tile impact
      const SVec2f origin_to_initial_impact = {
        initial_impact_position_raycast_x.x - raycast_origin.x,
        initial_impact_position_raycast_x.y - raycast_origin.y
      };
      const float origin_to_initial_impact_length = sqrtf(
        origin_to_initial_impact.x * origin_to_initial_impact.x +
        origin_to_initial_impact.y * origin_to_initial_impact.y
      );

      // Add points on vertical tile edges to list until out of grid bounds or ray length overshot
      while (true) {
        // Compute the impact point for the current step based on the initial impact
        // The step size is always the same for a given axis, so the triangle edges can be
        // determined from the grid tilesize and the ration of the raycast vector component lengths
        const SVec2f stepped_edge_impact = {
          initial_impact_position_raycast_x.x + step_size.x * step_index,
          initial_impact_position_raycast_x.y + step_size.y * step_index
        };

        // Stop stepping and ignore next impact point when the impact is out of grid bounds
        const bool edge_in_bounds = !(
          stepped_edge_impact.x < grid_bounding_box.min.x ||
          stepped_edge_impact.x > grid_bounding_box.max.x ||
          stepped_edge_impact.y < grid_bounding_box.min.y ||
          stepped_edge_impact.y > grid_bounding_box.max.y
        );
        if (!edge_in_bounds) break;

        // Stop stepping and ignore next impact point when the impact is further than the ray can reach
        const float step_length = sqrtf(step_size.x * step_size.x + step_size.y * step_size.y) * step_index;
        const float total_current_step_length = origin_to_initial_impact_length + step_length;
        const bool ray_reaches_impact = raycast_length >= total_current_step_length;
        if (!ray_reaches_impact) break;

        // Determine which tile the impact hits
        SVec2i tiled_impact_point = {
          stepped_edge_impact.x / gridTileDimensions.x,
          stepped_edge_impact.y / gridTileDimensions.y
        };
        SVec2i impacted_tile_based_on_ray = {
          raycast_vector.x >= 0 ? tiled_impact_point.x : tiled_impact_point.x - 1,
          tiled_impact_point.y
        };

        // Record the stepped impact point to the list of vertical impacts for the raycast x component
        if (points_recorded_raycast_x < MAX_POINTS_PER_AXIS) {
          const float stepped_edge_impact_time = total_current_step_length / raycast_length;
          p_info_raycast_x[points_recorded_raycast_x++] = (SImpactInformation) {
            stepped_edge_impact_time,
            stepped_edge_impact,
            impacted_tile_based_on_ray
          };
        }

        // Prepare for the next step
        step_index++;
      }
    }

    // Step through all impact points for horizontal tile edges
    if (raycast_direction.y != 0 && initial_ray_hits.y == true)
    {
      // Note: We know the y axis of the ray hit the horizontal tile edge containing the ray origin
      SVec2f initial_impact_position_raycast_y = {
        raycast_origin.x + raycast_vector.x * intersect_times_initial.y,
        raycast_origin.y + raycast_vector.y * intersect_times_initial.y
      };

      bool initial_impact_position_raycast_y_oob =
        initial_impact_position_raycast_y.x < grid_bounding_box.min.x ||
        initial_impact_position_raycast_y.x > grid_bounding_box.max.x ||
        initial_impact_position_raycast_y.y < grid_bounding_box.min.y ||
        initial_impact_position_raycast_y.y > grid_bounding_box.max.y;

      if (initial_impact_position_raycast_y_oob) return;

      // Determine which tile the impact hits
      SVec2i tiled_impact_point = {
        initial_impact_position_raycast_y.x / gridTileDimensions.x,
        tile_edge_in_ray_direction.y / gridTileDimensions.y
      };
      SVec2i impacted_tile_based_on_ray = {
        tiled_impact_point.x,
        raycast_vector.y >= 0 ? tiled_impact_point.y : tiled_impact_point.y - 1
      };

      // Record initial impact on vertical edge
      if (points_recorded_raycast_y < MAX_POINTS_PER_AXIS) {
        p_info_raycast_y[points_recorded_raycast_y++] = (SImpactInformation) {
          intersect_times_initial.y,
          initial_impact_position_raycast_y,
          impacted_tile_based_on_ray
        };
      }

      // Note: We know the horizontal edge hit is in bound of the grid
      // Now determine all subsequent impacts on the horizontal edges
      //
      // Do not accumulate the distances but rather determine the required lengths to avoid rounding errors
      // over large raycast distances
      int step_index = 1;
      const float vertical_edge_stepsize = fabs((raycast_vector.x * gridTileDimensions.y) / raycast_vector.y);
      const SVec2f step_size = { vertical_edge_stepsize * raycast_direction.x, gridTileDimensions.y * raycast_direction.y };

      // Account for the distance from origin to the initial tile impact
      const SVec2f origin_to_initial_impact = {
        initial_impact_position_raycast_y.x - raycast_origin.x,
        initial_impact_position_raycast_y.y - raycast_origin.y
      };
      const float origin_to_initial_impact_length = sqrtf(
        origin_to_initial_impact.x * origin_to_initial_impact.x +
        origin_to_initial_impact.y * origin_to_initial_impact.y
      );

      // Add points on horizontal tile edges to list until out of grid bounds or ray length overshot
      while (true) {
        // Compute the impact point for the current step based on the initial impact
        // The step size is always the same for a given axis, so the triangle edges can be
        // determined from the grid tilesize and the ration of the raycast vector component lengths
        const SVec2f stepped_edge_impact = {
          initial_impact_position_raycast_y.x + step_size.x * step_index,
          initial_impact_position_raycast_y.y + step_size.y * step_index
        };

        // Stop stepping and ignore next impact point when the impact is out of grid bounds
        const bool edge_in_bounds = !(
          stepped_edge_impact.x < grid_bounding_box.min.x ||
          stepped_edge_impact.x > grid_bounding_box.max.x ||
          stepped_edge_impact.y < grid_bounding_box.min.y ||
          stepped_edge_impact.y > grid_bounding_box.max.y
        );
        if (!edge_in_bounds) break;

        // Stop stepping and ignore next impact point when the impact is further than the ray can reach
        const float step_length = sqrtf(step_size.x * step_size.x + step_size.y * step_size.y) * step_index;
        const float total_current_step_length = origin_to_initial_impact_length + step_length;
        const bool ray_reaches_impact = raycast_length >= total_current_step_length;
        if (!ray_reaches_impact) break;

        // Determine which tile the impact hits
        SVec2i tiled_impact_point = {
          stepped_edge_impact.x / gridTileDimensions.x,
          stepped_edge_impact.y / gridTileDimensions.y
        };
        SVec2i impacted_tile_based_on_ray = {
          tiled_impact_point.x,
          raycast_vector.y >= 0 ? tiled_impact_point.y : tiled_impact_point.y - 1
        };

        // Record the stepped impact point to the list of horizontal impacts for the raycast x component
        if (points_recorded_raycast_y < MAX_POINTS_PER_AXIS) {
          const float stepped_edge_impact_time = total_current_step_length / raycast_length;
          p_info_raycast_y[points_recorded_raycast_y++] = (SImpactInformation) {
            stepped_edge_impact_time,
            stepped_edge_impact,
            impacted_tile_based_on_ray
          };
        }

        // Prepare for the next step
        step_index++;
      }
    }

  // Build a single single list of impact information from both sorted buffers
  // Are there any duplicates to worry about? If so, what is the causing edge-case?
  const int total_tiles_impacted = points_recorded_raycast_x + points_recorded_raycast_y;
  SImpactInformation * p_all_impact_infos_distance_ascending = malloc(sizeof(SImpactInformation) * total_tiles_impacted);
  int copy_index = 0;

  // Copy both axis impact buffers into a single one an sort the result
  for (int info_x_copy_index = 0; info_x_copy_index < points_recorded_raycast_x; info_x_copy_index++)
    p_all_impact_infos_distance_ascending[copy_index++] = p_info_raycast_x[info_x_copy_index];

  for (int info_y_copy_index = 0; info_y_copy_index < points_recorded_raycast_y; info_y_copy_index++)
    p_all_impact_infos_distance_ascending[copy_index++] = p_info_raycast_y[info_y_copy_index];

  qsort(p_all_impact_infos_distance_ascending, copy_index, sizeof(SImpactInformation), sort_impact_information);

  // Render all intersected times from lowest to highest impact time in ranging color
  SImpactInformation * p_current_info = NULL;
  const float color_index_step_increment = 1.0 / (float)total_tiles_impacted;
  for (int impact_info_index = 0; impact_info_index < total_tiles_impacted; impact_info_index++)
  {
    // Select single info for rendering
    p_current_info = p_all_impact_infos_distance_ascending + impact_info_index;

    // No batching - Just render for now
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
    glPointSize(6.0f);
    glBegin(GL_POINT);
      glVertex2f(p_current_info->impact_point.x, p_current_info->impact_point.y);
    glEnd();

    const float time_color = 1.0 - color_index_step_increment * impact_info_index;
    glColor4f(time_color, time_color, time_color, 1.0f);
    glRectf(
      p_current_info->impact_tile.x * gridTileDimensions.x,
      p_current_info->impact_tile.y * gridTileDimensions.y,
      p_current_info->impact_tile.x * gridTileDimensions.x + gridTileDimensions.x,
      p_current_info->impact_tile.y * gridTileDimensions.y + gridTileDimensions.y
    );
  }

  // Free used resources
  free(p_info_raycast_x);
  free(p_info_raycast_y);
  free(p_all_impact_infos_distance_ascending);
}

void render_scene(SSDL2SetupResult setup_result) {
  // Render the grid
  glLineWidth(1.0f);
  glColor4f(0.25f, 0.25f, 0.25f, 1.0f);
  glBegin(GL_LINES);
    // Horizontal grid lines from bottom to top
    for (int verticalGridLineIndex = 0; verticalGridLineIndex <= tilesOnGridAxis.y; verticalGridLineIndex++)
    {
      glVertex2i(gridOriginBottomLeft.x, gridOriginBottomLeft.y + verticalGridLineIndex * gridTileDimensions.y);
      glVertex2i(gridOriginBottomLeft.x + gridDimensions.x, gridOriginBottomLeft.y + verticalGridLineIndex * gridTileDimensions.y);
    }
    // Vertical grid lines from left to right
    for (int horizontalGridLineIndex = 0; horizontalGridLineIndex <= tilesOnGridAxis.x; horizontalGridLineIndex++)
    {
      glVertex2i(gridOriginBottomLeft.x + horizontalGridLineIndex * gridTileDimensions.x, gridOriginBottomLeft.y);
      glVertex2i(gridOriginBottomLeft.x + horizontalGridLineIndex * gridTileDimensions.x, gridOriginBottomLeft.y + gridDimensions.y);
    }
  glEnd();

  // Render grid border lines
  glLineWidth(1.0f);
  glColor4f(0.00f, 0.75f, 0.00, 1.0f);
  glBegin(GL_LINE_LOOP);
    glVertex2f(grid_bounding_box.min.x, grid_bounding_box.min.y);
    glVertex2f(grid_bounding_box.max.x, grid_bounding_box.min.y);
    glVertex2f(grid_bounding_box.max.x, grid_bounding_box.max.y);
    glVertex2f(grid_bounding_box.min.x, grid_bounding_box.max.y);
  glEnd();

  // Render raycasting origin and ray
  glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
  glLineWidth(2.0f);
  glBegin(GL_LINES);
    glVertex2f(raycast_origin.x, raycast_origin.y);
    glVertex2f(raycast_destination.x, raycast_destination.y);
  glEnd();

  glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
  glPointSize(12.0f);
  glBegin(GL_POINTS);
    glVertex2f(raycast_origin.x, raycast_origin.y);
  glEnd();
}

void gameloop(SSDL2SetupResult setup_result)
{
    // Gameloop state
    bool request_to_exit_application = false;

    while(!request_to_exit_application)
    {
      glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      poll_and_consume_input(&request_to_exit_application);

      update_scene();

      render_scene(setup_result);

      SDL_GL_SwapWindow(setup_result.p_window);
    }
}

void poll_and_consume_input(bool * request_to_exit_application)
{
  SDL_Event some_event;
  while(SDL_PollEvent(&some_event) != 0)
  {
    // There are events to consume
    // Keyboard Input
    switch (some_event.type) {
      case SDL_QUIT:
        *request_to_exit_application = true;
        break;
      case SDL_KEYDOWN:
        // All Keydown events
        switch (some_event.key.keysym.sym) {
          case SDLK_ESCAPE:
            *request_to_exit_application = true;
            break;
        }
        break;
    }
  }

  // Mouse state - Invert the Y axis to reflect OpenGL coordinates
  int mouse_x, mouse_y;
  Uint32 mouseButtonState = SDL_GetMouseState(&mouse_x, &mouse_y);
  mouse_y = SCREEN_HEIGHT - mouse_y;

  // Take over recorded input state to private compilation unit state
  mouse_screen_position = (SVec2i){mouse_x, mouse_y};
  mouse_left_button_held = ((mouseButtonState & SDL_BUTTON_LMASK) != 0) ? true : false;
  mouse_right_button_held = ((mouseButtonState & SDL_BUTTON_RMASK) != 0) ? true : false;
}
