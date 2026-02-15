import { startTransition } from 'react';
import { Pages, router, state } from '../App';
import { useSnapshot } from 'valtio';
import { SelectTabData, SelectTabEvent, Tab, TabList, OnNavItemSelectData } from '@fluentui/react-components';
import { NavDrawer } from '@fluentui/react-components';
import { NavDrawerBody } from '@fluentui/react-components';
import { NavItem } from '@fluentui/react-components';
import { FolderOpenRegular, PersonRegular, PeopleRegular, DocumentRegular, KeyRegular} from '@fluentui/react-icons';

export const NavSidebar = ({open }: {open: boolean}) => {
  const snap = useSnapshot(state);

  const onNavSelect = (event: any, data: OnNavItemSelectData) => {
    const nt = `/${data.value}`;
    startTransition(() => {
      router.navigate(nt);
    });
  };

  return (
    <>
    <NavDrawer open={open} modal={false} position="left" size="small" type="inline" selectedValue={snap.activePage} onNavItemSelect={onNavSelect}>
      <NavDrawerBody>
        <NavItem value={Pages.Buckets} icon={<FolderOpenRegular />}>
            Buckets
        </NavItem>
        <NavItem value={Pages.Users} icon={<PersonRegular />}>
            Users
        </NavItem>
        <NavItem value={Pages.Roles} icon={<PeopleRegular />}>
            Roles
        </NavItem>
        <NavItem value={Pages.Policies} icon={<DocumentRegular />}>
            Policies
        </NavItem>
      </NavDrawerBody>
    </NavDrawer>    
    </>
  );
};

export default NavSidebar;