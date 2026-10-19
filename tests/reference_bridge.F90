! Test-only ISO C bindings to the original TUV-x implementation.
! No reference source is linked into the production C library.
module reference_bridge
  use iso_c_binding
  implicit none
contains
  subroutine ref_interpolate(method, fold, nx, x, y, nt, target, out) bind(c)
    use tuvx_interpolate
    integer(c_int), value :: method, fold, nx, nt
    real(c_double), intent(in) :: x(nx), y(nx), target(nt)
    real(c_double), intent(out) :: out(*)
    class(interpolator_t), allocatable :: interp
    real(c_double), allocatable :: result(:)
    select case(method)
    case(0)
      allocate(interpolator_linear_t :: interp)
    case(1)
      allocate(interpolator_conserving_t :: interp)
    case(2)
      allocate(interpolator_fractional_source_t :: interp)
    case(3)
      allocate(interpolator_fractional_target_t :: interp)
    case default
      error stop 'unknown interpolation method'
    end select
    interp%fold_in_ = fold /= 0
    result = interp%interpolate(target, x, y, 'C roundoff comparison')
    out(1:size(result)) = result
  end subroutine

  subroutine ref_tridiagonal(n, a, b, c, rhs, out) bind(c)
    use tuvx_linear_algebra_linpack
    integer(c_int), value :: n
    real(c_double), intent(in) :: a(n), b(n), c(n), rhs(n)
    real(c_double), intent(out) :: out(n)
    type(linear_algebra_linpack_t) :: algebra
    out = algebra%tridiag(a, b, c, rhs)
  end subroutine

  subroutine ref_solar(year, day, hour, lat, lon, distance, elevation) bind(c)
    use tuvx_profile_utils
    integer(c_int), value :: year, day
    real(c_double), value :: hour, lat, lon
    real(c_double), intent(out) :: distance, elevation
    distance = earth_sun_distance(year, day, hour)
    elevation = solar_zenith_angle(year, day, hour, lat, lon)
  end subroutine

  subroutine ref_geometry(n, z, zen, air, nid, paths, vertical, slant) bind(c)
    use tuvx_grid
    use tuvx_grid_from_host
    use tuvx_grid_warehouse
    use tuvx_spherical_geometry
    integer(c_int), value :: n
    real(c_double), value :: zen
    real(c_double), intent(in) :: z(n), air(n)
    integer(c_int), intent(out) :: nid(n)
    real(c_double), intent(out) :: paths(n-1,n), vertical(n), slant(n)
    class(grid_warehouse_t), pointer :: grids
    class(grid_t), pointer :: grid
    type(spherical_geometry_t), pointer :: geometry
    integer :: i
    grids => grid_warehouse_t()
    grid => grid_from_host_t('height', 'km', n-1)
    grid%edge_ = z
    grid%mid_ = (z(:n-1) + z(2:)) * 0.5_c_double
    grid%delta_ = z(2:) - z(:n-1)
    call grids%add(grid)
    deallocate(grid)
    geometry => spherical_geometry_t(grids)
    call geometry%set_parameters(zen, grids)
    nid = geometry%nid_
    paths = transpose(geometry%dsdh_)
    ! The original air_mass leaves vertical(n) unspecified.
    vertical = 0.0_c_double
    call geometry%air_mass(air, vertical, slant)
    deallocate(geometry, grids)
  end subroutine
end module
