import { startTransition } from 'react';
import { Pages, router, state } from '../App';
import { useSnapshot } from 'valtio';
import { SelectTabData, SelectTabEvent, Tab, TabList } from '@fluentui/react-components';

export const NavSidebar = () => {
  const snap = useSnapshot(state);

  const onTabSelect = (event: SelectTabEvent, data: SelectTabData) => {
    const nt = `/${data.value}`;
    startTransition(() => {
      router.navigate(nt);
    });
  };

  return (
    <TabList selectedValue={snap.activePage} vertical onTabSelect={onTabSelect}>
      <Tab value={Pages.Buckets}>Buckets</Tab>
      <Tab value={Pages.Users}>Users</Tab>
      <Tab value={Pages.Roles}>Roles</Tab>
      <Tab value={Pages.Policies}>Policies</Tab>
      <Tab value={Pages.ChangePassword}>Change Password</Tab>
    </TabList>
  );
};

export default NavSidebar;