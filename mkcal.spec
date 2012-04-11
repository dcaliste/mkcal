# 
# Do NOT Edit the Auto-generated Part!
# Generated by: spectacle version 0.23
# 
# >> macros
%define ver_maj 0
%define ver_min 3
%define ver_pat 11
# << macros

Name:       mkcal
Summary:    Extended KDE kcal calendar library port for Maemo
Version:    %{ver_maj}.%{ver_min}.%{ver_pat}
Release:    1
Group:      System/Libraries
License:    LGPLv2
Source0:    %{name}-%{version}.tar.gz
Source100:  mkcal.yaml
Patch0:     mkcal-0.3.8-no-mtf.patch
Patch1:     mkcal-0.3.11-no-timed.patch
Patch2:     mkcal-0.3.11-disable-plugins.patch
Patch3:     mkcal-0.3.11-drop-tracker.patch
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(QtCore)
BuildRequires:  pkgconfig(libkcalcoren)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(uuid)
BuildRequires:  doxygen
BuildRequires:  fdupes


%description
%{summary}.


%package devel
Summary:    Development files for %{name}
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
%{summary}.


%prep
%setup -q -n %{name}-%{version}

# mkcal-0.3.8-no-mtf.patch
%patch0 -p1
# mkcal-0.3.11-no-timed.patch
%patch1 -p1
# mkcal-0.3.11-disable-plugins.patch
%patch2 -p1
# mkcal-0.3.11-drop-tracker.patch
%patch3 -p1
# >> setup
# << setup

%build
# >> build pre
touch src/libmkcal.so.%{ver_maj}.%{ver_min}.%{ver_pat}
%qmake INCLUDEPATH="%{_includedir}/kcalcoren %{_includedir}/QtGui"
make VER_MAJ=%{ver_maj} VER_MIN=%{ver_min} VER_PAT=%{ver_pat} %{?_smp_mflags}
# << build pre



# >> build post
# << build post
%install
rm -rf %{buildroot}
# >> install pre
# << install pre

# >> install post
%qmake_install VER_MAJ=%{ver_maj} VER_MIN=%{ver_min} VER_PAT=%{ver_pat}
ln -s libmkcal.so.%{ver_maj}.%{ver_min}.%{ver_pat} %{buildroot}%{_libdir}/libmkcal.so
ln -s libmkcal.so.%{ver_maj}.%{ver_min}.%{ver_pat} %{buildroot}%{_libdir}/libmkcal.so.%{ver_maj}
ln -s libmkcal.so.%{ver_maj}.%{ver_min}.%{ver_pat} %{buildroot}%{_libdir}/libmkcal.so.%{ver_maj}.%{ver_min}
chmod +x %{buildroot}/%{_libdir}/libmkcal.so.%{ver_maj}.%{ver_min}.%{ver_pat}
%fdupes %{buildroot}%{_docdir}

# << install post



%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig





%files
%defattr(-,root,root,-)
# >> files
%defattr(644,root,root,-)
%{_libdir}/libmkcal.so.*
# << files


%files devel
%defattr(-,root,root,-)
# >> files devel
%{_includedir}/mkcal/*.h
%{_libdir}/libmkcal.so
%{_libdir}/pkgconfig/libmkcal.pc
%{_datadir}/qt4/mkspecs/features/mkcal.prf
# << files devel

